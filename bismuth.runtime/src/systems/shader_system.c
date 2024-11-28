#include "shader_system.h"

#include "containers/darray.h"
#include "core/engine.h"
#include "core/event.h"
#include "core_render_types.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_utils.h"
#include "resources/resource_types.h"
#include "strings/bname.h"
#include "strings/bstring.h"
#include "systems/resource_system.h"
#include "systems/texture_system.h"

/** @brief Represents a shader on the frontend. This is internal to the shader system */
typedef struct bshader
{
    /** @brief unique identifier that is compared against a handle */
    u64 uniqueid;

    bname name;

    shader_flag_bits flags;

    /** @brief The types of topologies used by the shader and its pipeline. See primitive_topology_type */
    u32 topology_types;

    /** @brief An array of uniforms in this shader. Darray */
    shader_uniform* uniforms;

    /** @brief An array of attributes. Darray */
    shader_attribute* attributes;

    /** @brief The size of all attributes combined, a.k.a. the size of a vertex */
    u16 attribute_stride;

    u8 shader_stage_count;
    shader_stage_config* stage_configs;

    /** @brief Per-frame frequency data */
    shader_frequency_data per_frame;
    /** @brief Per-group frequency data */
    shader_frequency_data per_group;
    /** @brief Per-draw frequency data */
    shader_frequency_data per_draw;

    /** @brief The internal state of the shader */
    shader_state state;

#ifdef _DEBUG
    u32* module_watch_ids;
#endif
} bshader;

// Internal shader system state
typedef struct shader_system_state
{
    // A pointer to the renderer system state
    struct renderer_system_state* renderer;
    struct texture_system_state* texture_system;

    // The max number of textures that can be bound for a single draw call, provided by the renderer
    u16 max_bound_texture_count;
    // The max number of samplers that can be bound for a single draw call, provided by the renderer
    u16 max_bound_sampler_count;

    // This system's configuration
    shader_system_config config;
    // A collection of created shaders
    bshader* shaders;
} shader_system_state;

// Pointer to hold the internal system state
static shader_system_state* state_ptr = 0;

static b8 internal_attribute_add(bshader* shader, const shader_attribute_config* config);
static b8 internal_texture_add(bshader* shader, shader_uniform_config* config);
static b8 internal_sampler_add(bshader* shader, shader_uniform_config* config);
static bhandle generate_new_shader_handle(void);
static b8 internal_uniform_add(bshader* shader, const shader_uniform_config* config, u32 location);

// Verify the name is valid and unique
static b8 uniform_name_valid(bshader* shader, bname uniform_name);
static b8 shader_uniform_add_state_valid(bshader* shader);
static void internal_shader_destroy(bhandle* shader);

#ifdef _DEBUG
static b8 file_watch_event(u16 code, void* sender, void* listener_inst, event_context context)
{
    shader_system_state* typed_state = (shader_system_state*)listener_inst;
    if (code == EVENT_CODE_WATCHED_FILE_WRITTEN)
    {
        u32 file_watch_id = context.data.u32[0];

        // Search shaders for the one with the changed file watch id
        for (u32 i = 0; i < typed_state->config.max_shader_count; ++i)
        {
            bshader* s = &typed_state->shaders[i];
            for (u32 w = 0; w < s->shader_stage_count; ++w)
            {
                if (s->module_watch_ids[w] == file_watch_id)
                {
                    bhandle handle = bhandle_create_with_u64_identifier(i, s->uniqueid);
                    if (!shader_system_reload(handle))
                    {
                        BWARN("Shader hot-reload failed for shader '%s'. See logs for details", s->name);
                        // Allow other systems to pick this up
                        return false;
                    }
                }
            }
        }
    }

    // Return as unhandled to allow other systems to pick it up
    return false;
}
#endif

b8 shader_system_initialize(u64* memory_requirement, void* memory, void* config)
{
    shader_system_config* typed_config = (shader_system_config*)config;
    // Verify configuration
    if (typed_config->max_shader_count < 512)
    {
        if (typed_config->max_shader_count == 0)
        {
            BERROR("shader_system_initialize - typed_config->max_shader_count must be greater than 0. Defaulting to 512");
            typed_config->max_shader_count = 512;
        }
        else
        {
            BWARN("shader_system_initialize - typed_config->max_shader_count is recommended to be at least 512");
        }
    }

    // Block of memory will contain state structure then the block for the shader array
    u64 struct_requirement = sizeof(shader_system_state);
    u64 shader_array_requirement = sizeof(bshader) * typed_config->max_shader_count;
    *memory_requirement = struct_requirement + shader_array_requirement;

    if (!memory)
        return true;

    // Setup the state pointer, memory block, shader array, then create the hashtable
    state_ptr = memory;
    u64 addr = (u64)memory;
    state_ptr->shaders = (void*)(addr + struct_requirement);
    state_ptr->config = *typed_config;

    // Invalidate all shader ids
    for (u32 i = 0; i < typed_config->max_shader_count; ++i)
    {
        state_ptr->shaders[i].uniqueid = INVALID_ID_U64;
    }
    
    // Keep a pointer to the renderer state.
    state_ptr->renderer = engine_systems_get()->renderer_system;
    state_ptr->texture_system = engine_systems_get()->texture_system;

    // Track max texture and sampler counts
    state_ptr->max_bound_sampler_count = renderer_max_bound_sampler_count_get(state_ptr->renderer);
    state_ptr->max_bound_texture_count = renderer_max_bound_texture_count_get(state_ptr->renderer);
    
#ifdef _DEBUG
    // Watch for file hot reloads in debug builds
    event_register(EVENT_CODE_WATCHED_FILE_WRITTEN, state_ptr, file_watch_event);
#endif

    return true;
}

void shader_system_shutdown(void* state)
{
    if (state)
    {
        // Destroy any shaders still in existence
        shader_system_state* st = (shader_system_state*)state;
        for (u32 i = 0; i < st->config.max_shader_count; ++i)
        {
            bshader* s = &st->shaders[i];
            if (s->uniqueid != INVALID_ID_U64)
            {
                bhandle temp_handle = bhandle_create_with_u64_identifier(i, s->uniqueid);
                internal_shader_destroy(&temp_handle);
            }
        }
        bzero_memory(st, sizeof(shader_system_state));
    }

    state_ptr = 0;
}

bhandle shader_system_create(const shader_config* config)
{
    bhandle new_handle = generate_new_shader_handle();
    if (bhandle_is_invalid(new_handle))
    {
        BERROR("Unable to find free slot to create new shader. Aborting");
        return new_handle;
    }

    bshader* out_shader = &state_ptr->shaders[new_handle.handle_index];
    bzero_memory(out_shader, sizeof(bshader));
    // Sync handle uniqueid
    out_shader->uniqueid = new_handle.unique_id.uniqueid;
    out_shader->state = SHADER_STATE_NOT_CREATED;
    out_shader->name = bname_create(config->name);
    out_shader->attribute_stride = 0;
    out_shader->shader_stage_count = config->stage_count;
    out_shader->stage_configs = ballocate(sizeof(shader_stage_config) * config->stage_count, MEMORY_TAG_ARRAY);
    out_shader->uniforms = darray_create(shader_uniform);
    out_shader->attributes = darray_create(shader_attribute);

    // Per-frame frequency
    out_shader->per_frame.bound_id = INVALID_ID; // NOTE: per-frame doesn't have a bound id, but invalidate it anyway
    out_shader->per_frame.uniform_count = 0;
    out_shader->per_frame.uniform_sampler_count = 0;
    out_shader->per_frame.sampler_indices = 0;
    out_shader->per_frame.uniform_texture_count = 0;
    out_shader->per_frame.texture_indices = 0;
    out_shader->per_frame.ubo_size = 0;

    // Per-group frequency
    out_shader->per_group.bound_id = INVALID_ID;
    out_shader->per_group.uniform_count = 0;
    out_shader->per_group.uniform_sampler_count = 0;
    out_shader->per_group.sampler_indices = 0;
    out_shader->per_group.uniform_texture_count = 0;
    out_shader->per_group.texture_indices = 0;
    out_shader->per_group.ubo_size = 0;

    // Per-draw frequency
    out_shader->per_draw.bound_id = INVALID_ID;
    out_shader->per_draw.uniform_count = 0;
    out_shader->per_group.uniform_sampler_count = 0;
    out_shader->per_group.sampler_indices = 0;
    out_shader->per_group.uniform_texture_count = 0;
    out_shader->per_group.texture_indices = 0;
    // TODO: per-draw frequency does not have a UBO. To provided by the renderer
    out_shader->per_draw.ubo_size = 0;

    // Take copy of the flags
    out_shader->flags = config->flags;

#ifdef _DEBUG
    // NOTE: Only watch module files for debug builds
    out_shader->module_watch_ids = ballocate(sizeof(u32) * config->stage_count, MEMORY_TAG_ARRAY);
#endif

    // Examine shader stages and load shader source as required.
    // This source is then fed to the backend renderer, which stands up any shader program resources as required
    // TODO: Implement #include directives here at this level so it's handled the same regardless of what backend is being used
    // Each stage
    for (u8 i = 0; i < config->stage_count; ++i)
    {
        out_shader->stage_configs[i].stage = config->stage_configs[i].stage;
        out_shader->stage_configs[i].filename = string_duplicate(config->stage_configs[i].filename);
        // FIXME: Convert to use the new resource system
        // Read the resource
        resource text_resource;
        if (!resource_system_load(out_shader->stage_configs[i].filename, RESOURCE_TYPE_TEXT, 0, &text_resource))
        {
            BERROR("Unable to read shader file: %s", out_shader->stage_configs[i].filename);
            // Invalidate the new handle and return it
            bhandle_invalidate(&new_handle);
            return new_handle;
        }
        // Take a copy of the source and length, then release the resource
        // out_shader->stage_configs[i].source_length = text_resource.data_size;
        out_shader->stage_configs[i].source = string_duplicate(text_resource.data);
        // TODO: Implement recursive #include directives here at this level so it's handled the same regardless of what backend is being used
        // This should recursively replace #includes with the file content in-place and adjust the source length along the way

#ifdef _DEBUG
        // Allow shader hot-reloading in debug builds
        if (!platform_watch_file(text_resource.full_path, &out_shader->module_watch_ids[i]))
        {
            // If this fails, warn about it but there's no need to crash over it
            BWARN("Failed to watch shader source file '%s'", text_resource.full_path);
        }
#endif

        // Release the resource as it isn't needed anymore at this point
        resource_system_unload(&text_resource);
    }

    // Keep a copy of the topology types
    out_shader->topology_types = config->topology_types;

    // Ready to be initialized
    out_shader->state = SHADER_STATE_UNINITIALIZED;

    // Process attributes
    for (u32 i = 0; i < config->attribute_count; ++i)
    {
        shader_attribute_config* ac = &config->attributes[i];
        if (!internal_attribute_add(out_shader, ac))
        {
            BERROR("Failed to add attribute '%s' to shader '%s'", ac->name, config->name);
            // Invalidate the new handle and return it
            bhandle_invalidate(&new_handle);
            return new_handle;
        }
    }

    // Process uniforms
    for (u32 i = 0; i < config->uniform_count; ++i)
    {
        shader_uniform_config* uc = &config->uniforms[i];
        b8 uniform_add_result = false;
        if (uniform_type_is_sampler(uc->type))
        {
            uniform_add_result = internal_sampler_add(out_shader, uc);
        }
        else if (uniform_type_is_texture(uc->type))
        {
            uniform_add_result = internal_texture_add(out_shader, uc);
        }
        else
        {
            uniform_add_result = internal_uniform_add(out_shader, uc, INVALID_ID);
        }
        if (!uniform_add_result)
        {
            // Invalidate the new handle and return it
            bhandle_invalidate(&new_handle);
            return new_handle;
        }
    }

    // Now that uniforms are processed, take note of the indices of textures and samplers
    // These are used for fast lookups later by type
    out_shader->per_frame.sampler_indices = BALLOC_TYPE_CARRAY(u32, out_shader->per_frame.uniform_sampler_count);
    out_shader->per_group.sampler_indices = BALLOC_TYPE_CARRAY(u32, out_shader->per_group.uniform_sampler_count);
    out_shader->per_draw.sampler_indices = BALLOC_TYPE_CARRAY(u32, out_shader->per_draw.uniform_sampler_count);
    u32 frame_textures = 0, frame_samplers = 0;
    u32 group_textures = 0, group_samplers = 0;
    u32 draw_textures = 0, draw_samplers = 0;
    for (u32 i = 0; i < config->uniform_count; ++i)
    {
        shader_uniform_config* uc = &config->uniforms[i];
        if (uniform_type_is_sampler(uc->type))
        {
            switch (uc->frequency)
            {
            case SHADER_UPDATE_FREQUENCY_PER_FRAME:
                out_shader->per_frame.sampler_indices[frame_samplers] = i;
                break;
            case SHADER_UPDATE_FREQUENCY_PER_GROUP:
                out_shader->per_group.sampler_indices[group_samplers] = i;
                break;
            case SHADER_UPDATE_FREQUENCY_PER_DRAW:
                out_shader->per_draw.sampler_indices[draw_samplers] = i;
                break;
            }
        }
        else if (uniform_type_is_texture(uc->type))
        {
            switch (uc->frequency)
            {
            case SHADER_UPDATE_FREQUENCY_PER_FRAME:
                out_shader->per_frame.texture_indices[frame_textures] = i;
                break;
            case SHADER_UPDATE_FREQUENCY_PER_GROUP:
                out_shader->per_group.texture_indices[group_textures] = i;
                break;
            case SHADER_UPDATE_FREQUENCY_PER_DRAW:
                out_shader->per_draw.texture_indices[draw_textures] = i;
                break;
            }
        }
    }

    // Create renderer-internal resources
    if (!renderer_shader_create(state_ptr->renderer, new_handle, config))
    {
        BERROR("Error creating shader");
        // Invalidate the new handle and return it
        bhandle_invalidate(&new_handle);
        return new_handle;
    }

    return new_handle;
}

b8 shader_system_reload(bhandle shader)
{
    if (bhandle_is_invalid(shader))
        return false;

    bshader* s = &state_ptr->shaders[shader.handle_index];

    // Make a copy of the stage configs in case a file fails to load
    b8 has_error = false;
    shader_stage_config* new_stage_configs = ballocate(sizeof(shader_stage_config) * s->shader_stage_count, MEMORY_TAG_ARRAY);
    for (u8 i = 0; i < s->shader_stage_count; ++i)
    {
        // Read the resource
        resource text_resource;
        if (!resource_system_load(s->stage_configs[i].filename, RESOURCE_TYPE_TEXT, 0, &text_resource))
        {
            BERROR("Unable to read shader file: %s", s->stage_configs[i].filename);
            has_error = true;
            break;
        }

        // Free the old source
        if (s->stage_configs[i].source)
            string_free(s->stage_configs[i].source);

        // Take a copy of the source and length, then release the resource
        new_stage_configs[i].source = string_duplicate(text_resource.data);
        // TODO: Implement recursive #include directives here at this level so it's handled the same regardless of what backend is being used
        // This should recursively replace #includes with the file content in-place and adjust the source length along the way

        // Release the resource as it isn't needed anymore at this point
        resource_system_unload(&text_resource);
    }

    for (u8 i = 0; i < s->shader_stage_count; ++i)
    {
        if (has_error)
        {
            if (new_stage_configs[i].source)
                string_free(new_stage_configs[i].source);
        }
        else
        {
            s->stage_configs[i].source = new_stage_configs[i].source;
        }
    }
    bfree(new_stage_configs, sizeof(shader_stage_config) * s->shader_stage_count, MEMORY_TAG_ARRAY);
    if (has_error)
        return false;

    return renderer_shader_reload(state_ptr->renderer, shader, s->shader_stage_count, s->stage_configs);
}

bhandle shader_system_get(bname name)
{
    if (name == INVALID_BNAME)
        return bhandle_invalid();

    u32 count = state_ptr->config.max_shader_count;
    for (u32 i = 0; i < count; ++i)
    {
        if (state_ptr->shaders[i].name == name)
        {
            return bhandle_create_with_u64_identifier(i, state_ptr->shaders[i].uniqueid);
        }
    }

    // Not found, attempt to load the shader resource and return it
    resource shader_config_resource;
    if (!resource_system_load(bname_string_get(name), RESOURCE_TYPE_SHADER, 0, &shader_config_resource))
    {
        BERROR("Failed to load shader resource for shader '%s'", name);
        return bhandle_invalid();
    }

    shader_config* config = (shader_config*)shader_config_resource.data;
    bhandle shader_handle = shader_system_create(config);
    resource_system_unload(&shader_config_resource);

    if (bhandle_is_invalid(shader_handle))
    {
        BERROR("Failed to create shader '%s'", name);
        BERROR("There is no shader available called '%s', and one by that name could also not be loaded", bname_string_get(name));
        return shader_handle;
    }

    return shader_handle;
}

static void internal_shader_destroy(bhandle* shader)
{
    if (bhandle_is_invalid(*shader) || bhandle_is_stale(*shader, state_ptr->shaders[shader->handle_index].uniqueid))
        return;

    renderer_shader_destroy(state_ptr->renderer, *shader);

    bshader* s = &state_ptr->shaders[shader->handle_index];

    // Set it to be unusable right away
    s->state = SHADER_STATE_NOT_CREATED;

    s->name = INVALID_BNAME;

#ifdef _DEBUG
    if (s->module_watch_ids)
    {
        // Unwatch the shader files
        for (u8 i = 0; i < s->shader_stage_count; ++i)
            platform_unwatch_file(s->module_watch_ids[i]);
    }
#endif

    // Make sure to invalidate the handle
    bhandle_invalidate(shader);
}

void shader_system_destroy(bhandle* shader)
{
    if (bhandle_is_invalid(*shader))
        return;

    internal_shader_destroy(shader);
}

b8 shader_system_set_wireframe(bhandle shader, b8 wireframe_enabled)
{
    if (bhandle_is_invalid(shader))
    {
        BERROR("Invalid shader passed");
        return false;
    }

    if (!wireframe_enabled)
    {
        renderer_shader_flag_set(state_ptr->renderer, shader, SHADER_FLAG_WIREFRAME, false);
        return true;
    }

    if (renderer_shader_supports_wireframe(state_ptr->renderer, shader))
        renderer_shader_flag_set(state_ptr->renderer, shader, SHADER_FLAG_WIREFRAME, true);

    return true;
}

b8 shader_system_use(bhandle shader)
{
    if (bhandle_is_invalid(shader))
    {
        BERROR("Invalid shader passed");
        return false;
    }
    bshader* next_shader = &state_ptr->shaders[shader.handle_index];
    if (!renderer_shader_use(state_ptr->renderer, shader))
    {
        BERROR("Failed to use shader '%s'", next_shader->name);
        return false;
    }

    return true;
}

u16 shader_system_uniform_location(bhandle shader, bname uniform_name)
{
    if (bhandle_is_invalid(shader))
    {
        BERROR("Invalid shader passed");
        return INVALID_ID_U16;
    }
    bshader* next_shader = &state_ptr->shaders[shader.handle_index];

    u32 uniform_count = darray_length(next_shader->uniforms);
    for (u32 i = 0; i < uniform_count; ++i)
    {
        if (next_shader->uniforms[i].name == uniform_name)
            return next_shader->uniforms[i].location;
    }

    // Not found
    return INVALID_ID_U16;
}

b8 shader_system_uniform_set(bhandle shader, bname uniform_name, const void* value)
{
    return shader_system_uniform_set_arrayed(shader, uniform_name, 0, value);
}

b8 shader_system_uniform_set_arrayed(bhandle shader, bname uniform_name, u32 array_index, const void* value)
{
    if (bhandle_is_invalid(shader))
    {
        BERROR("shader_system_uniform_set_arrayed called with invalid shader handle");
        return false;
    }

    u16 index = shader_system_uniform_location(shader, uniform_name);
    return shader_system_uniform_set_by_location_arrayed(shader, index, array_index, value);
}

b8 shader_system_texture_set(bhandle shader, bname sampler_name, const bresource_texture* t)
{
    return shader_system_texture_set_arrayed(shader, sampler_name, 0, t);
}

b8 shader_system_texture_set_arrayed(bhandle shader, bname sampler_name, u32 array_index, const bresource_texture* t)
{
    return shader_system_uniform_set_arrayed(shader, sampler_name, array_index, t);
}

b8 shader_system_texture_set_by_location(bhandle shader, u16 location, const bresource_texture* t)
{
    return shader_system_uniform_set_by_location_arrayed(shader, location, 0, t);
}

b8 shader_system_uniform_set_by_location(bhandle shader, u16 location, const void* value)
{
    return shader_system_uniform_set_by_location_arrayed(shader, location, 0, value);
}

b8 shader_system_uniform_set_by_location_arrayed(bhandle shader, u16 location, u32 array_index, const void* value)
{
    bshader* s = &state_ptr->shaders[shader.handle_index];
    shader_uniform* uniform = &s->uniforms[location];
    return renderer_shader_uniform_set(state_ptr->renderer, shader, uniform, array_index, value);
}

b8 shader_system_bind_frame(bhandle shader)
{
    if (bhandle_is_invalid(shader) || bhandle_is_stale(shader, state_ptr->shaders[shader.handle_index].uniqueid))
    {
        BERROR("Tried to bind_frame on a shader using an invalid or stale handle. Nothing to be done");
        return false;
    }
    return renderer_shader_bind_per_frame(state_ptr->renderer, shader);
}

b8 shader_system_bind_group(bhandle shader, u32 group_id)
{
    if (group_id == INVALID_ID)
    {
        BERROR("Cannot bind shader instance INVALID_ID");
        return false;
    }
    state_ptr->shaders[shader.handle_index].per_group.bound_id = group_id;
    return renderer_shader_bind_per_group(state_ptr->renderer, shader, group_id);
}

b8 shader_system_bind_draw_id(bhandle shader, u32 draw_id)
{
    if (draw_id == INVALID_ID)
    {
        BERROR("Cannot bind shader local id INVALID_ID");
        return false;
    }
    state_ptr->shaders[shader.handle_index].per_draw.bound_id = draw_id;
    return renderer_shader_bind_per_draw(state_ptr->renderer, shader, draw_id);
}

b8 shader_system_apply_per_frame(bhandle shader)
{
    return renderer_shader_apply_per_frame(state_ptr->renderer, shader);
}

b8 shader_system_apply_per_group(bhandle shader)
{
    return renderer_shader_apply_per_group(state_ptr->renderer, shader);
}

b8 shader_system_apply_per_draw(bhandle shader)
{
    return renderer_shader_apply_per_draw(state_ptr->renderer, shader);
}

b8 shader_system_shader_group_acquire(bhandle shader, u32* out_group_id)
{
    return renderer_shader_per_group_resources_acquire(state_ptr->renderer, shader, out_group_id);
}

b8 shader_system_shader_per_draw_acquire(bhandle shader, u32* out_per_draw_id)
{
    return renderer_shader_per_draw_resources_acquire(state_ptr->renderer, shader, out_per_draw_id);
}

b8 shader_system_shader_group_release(bhandle shader, u32 group_id)
{
    return renderer_shader_per_group_resources_release(state_ptr->renderer, shader, group_id);
}

b8 shader_system_shader_per_draw_release(bhandle shader, u32 per_draw_id)
{
    return renderer_shader_per_draw_resources_release(state_ptr->renderer, shader, per_draw_id);
}

static b8 internal_attribute_add(bshader* shader, const shader_attribute_config* config)
{
    u32 size = 0;
    switch (config->type)
    {
        case SHADER_ATTRIB_TYPE_INT8:
        case SHADER_ATTRIB_TYPE_UINT8:
            size = 1;
            break;
        case SHADER_ATTRIB_TYPE_INT16:
        case SHADER_ATTRIB_TYPE_UINT16:
            size = 2;
            break;
        case SHADER_ATTRIB_TYPE_FLOAT32:
        case SHADER_ATTRIB_TYPE_INT32:
        case SHADER_ATTRIB_TYPE_UINT32:
            size = 4;
            break;
        case SHADER_ATTRIB_TYPE_FLOAT32_2:
            size = 8;
            break;
        case SHADER_ATTRIB_TYPE_FLOAT32_3:
            size = 12;
            break;
        case SHADER_ATTRIB_TYPE_FLOAT32_4:
            size = 16;
            break;
        default:
            BERROR("Unrecognized type %d, defaulting to size of 4. This probably is not what is desired");
            size = 4;
            break;
    }

    shader->attribute_stride += size;

    // Create/push the attribute
    shader_attribute attrib = {};
    attrib.name = bname_create(config->name);
    attrib.size = size;
    attrib.type = config->type;
    darray_push(shader->attributes, attrib);

    return true;
}

static b8 internal_texture_add(bshader* shader, shader_uniform_config* config)
{
    // Verify the name is valid and unique
    if (!uniform_name_valid(shader, bname_create(config->name)) || !shader_uniform_add_state_valid(shader))
        return false;
    
    // Verify that there are not too many textures present across all frequencies
    u16 current_texture_count = shader->per_frame.uniform_texture_count + shader->per_group.uniform_texture_count + shader->per_draw.uniform_texture_count;
    if (current_texture_count + 1 > state_ptr->max_bound_texture_count)
    {
        BERROR("Cannot add another texture uniform to shader '%s' as it has already reached the maximum per-draw bound total of %hu", bname_string_get(shader->name), state_ptr->max_bound_texture_count);
        return false;
    }

    // If per-draw, push into per-frame list
    u32 location = 0;
    if (config->frequency == SHADER_UPDATE_FREQUENCY_PER_FRAME)
    {
        location = shader->per_frame.uniform_texture_count;
        shader->per_frame.uniform_texture_count++;
    }
    else if (config->frequency == SHADER_UPDATE_FREQUENCY_PER_GROUP)
    {
        location = shader->per_group.uniform_texture_count;
        shader->per_group.uniform_texture_count++;
    }
    else if (config->frequency == SHADER_UPDATE_FREQUENCY_PER_DRAW)
    {
        location = shader->per_draw.uniform_texture_count;
        shader->per_draw.uniform_texture_count++;
    }

    // Treat it like a uniform
    // NOTE: In the case of textures, location is used to determine the entry's 'location' field value directly, and is then set to the index of the uniform array.
    // This allows location lookups for textures as if they were uniforms as well (since technically they are)
    if (!internal_uniform_add(shader, config, location))
    {
        BERROR("Unable to add texture uniform");
        return false;
    }

    return true;
}

static b8 internal_sampler_add(bshader* shader, shader_uniform_config* config)
{
    // Verify the name is valid and unique
    if (!uniform_name_valid(shader, bname_create(config->name)) || !shader_uniform_add_state_valid(shader))
        return false;

    // Verify that there are not too many samplers present across all frequencies
    u16 current_sampler_count = shader->per_frame.uniform_sampler_count + shader->per_group.uniform_sampler_count + shader->per_draw.uniform_sampler_count;
    if (current_sampler_count + 1 > state_ptr->max_bound_sampler_count)
    {
        BERROR("Cannot add another sampler uniform to shader '%s' as it has already reached the maximum per-draw bound total of %hu", bname_string_get(shader->name), state_ptr->max_bound_sampler_count);
        return false;
    }

    // If per-frame, push into the per-frame list
    u32 location = 0;
    if (config->frequency == SHADER_UPDATE_FREQUENCY_PER_FRAME)
    {
        location = shader->per_frame.uniform_sampler_count;
        shader->per_frame.uniform_sampler_count++;
    }
    else if (config->frequency == SHADER_UPDATE_FREQUENCY_PER_GROUP)
    {
        location = shader->per_group.uniform_sampler_count;
        shader->per_group.uniform_sampler_count++;
    }
    else if (config->frequency == SHADER_UPDATE_FREQUENCY_PER_DRAW)
    {
        location = shader->per_draw.uniform_sampler_count;
        shader->per_draw.uniform_sampler_count++;
    }

    // Treat it like a uniform
    // NOTE: In the case of samplers, location is used to determine the entry's 'location' field value directly, and is then set to the index of the uniform array.
    // This allows location lookups for samplers as if they were uniforms as well (since technically they are)
    if (!internal_uniform_add(shader, config, location))
    {
        BERROR("Unable to add sampler uniform");
        return false;
    }

    return true;
}

static bhandle generate_new_shader_handle(void)
{
    for (u32 i = 0; i < state_ptr->config.max_shader_count; ++i)
    {
        if (state_ptr->shaders[i].uniqueid == INVALID_ID_U64)
            return bhandle_create(i);
    }
    return bhandle_invalid();
}

static b8 internal_uniform_add(bshader* shader, const shader_uniform_config* config, u32 location)
{
    if (!shader_uniform_add_state_valid(shader) || !uniform_name_valid(shader, bname_create(config->name)))
        return false;

    u32 uniform_count = darray_length(shader->uniforms);
    if (uniform_count + 1 > state_ptr->config.max_uniform_count)
    {
        BERROR("Shader can only accept a combined maximum of %d uniforms and samplers at global, instance and local scopes", state_ptr->config.max_uniform_count);
        return false;
    }
    b8 is_sampler_or_texture = uniform_type_is_sampler(config->type) || uniform_type_is_texture(config->type);
    shader_uniform entry;
    entry.frequency = config->frequency;
    entry.type = config->type;
    entry.array_length = config->array_length;
    b8 is_per_frame = (config->frequency == SHADER_UPDATE_FREQUENCY_PER_FRAME);
    if (is_sampler_or_texture)
    {
        // Use passed in location
        entry.location = location;
    }
    else
    {
        // Otherwise for regular non-texture/non-sampler uniforms, the location is just the index in the array
        entry.location = uniform_count;
    }

    if (config->frequency == SHADER_UPDATE_FREQUENCY_PER_DRAW)
    {
        entry.offset = shader->per_draw.ubo_size;
        entry.size = config->size;
    }
    else
    {
        entry.offset = is_sampler_or_texture ? 0 : is_per_frame ? shader->per_frame.ubo_size
                                                                : shader->per_group.ubo_size;
        entry.size = is_sampler_or_texture ? 0 : config->size;
    }

    darray_push(shader->uniforms, entry);

    // Count regular uniforms only, as the others are counted in the functions called before this for textures and samplers
    if (!is_sampler_or_texture)
    {
        if (entry.frequency == SHADER_UPDATE_FREQUENCY_PER_FRAME)
        {
            shader->per_frame.ubo_size += (entry.size * entry.array_length);
            shader->per_frame.uniform_count++;
        }
        else if (entry.frequency == SHADER_UPDATE_FREQUENCY_PER_GROUP)
        {
            shader->per_group.ubo_size += (entry.size * entry.array_length);
            shader->per_group.uniform_count++;
        }
        else if (entry.frequency == SHADER_UPDATE_FREQUENCY_PER_DRAW)
        {
            shader->per_draw.ubo_size += (entry.size * entry.array_length);
            shader->per_draw.uniform_count++;
        }
    }

    return true;
}

static b8 uniform_name_valid(bshader* shader, bname uniform_name)
{
    if (uniform_name == INVALID_BNAME)
    {
        BERROR("Uniform name is invalid");
        return false;
    }
    u32 uniform_count = darray_length(shader->uniforms);
    for (u32 i = 0; i < uniform_count; ++i)
    {
        if (shader->uniforms[i].name == uniform_name)
        {
            BERROR("A uniform by the name '%s' already exists on shader '%s'", uniform_name, shader->name);
            return false;
        }
    }

    return true;
}

static b8 shader_uniform_add_state_valid(bshader* shader)
{
    if (shader->state != SHADER_STATE_UNINITIALIZED)
    {
        BERROR("Uniforms may only be added to shaders before initialization");
        return false;
    }
    return true;
}
