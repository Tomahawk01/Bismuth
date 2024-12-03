#include "material_system.h"

#include "assets/basset_types.h"
#include "containers/darray.h"
#include "core/console.h"
#include "core/engine.h"
#include "core/frame_data.h"
#include "debug/bassert.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "renderer/rendergraph_nodes/shadow_rendergraph_node.h"
#include "serializers/basset_material_serializer.h"
#include "strings/bname.h"
#include "systems/bresource_system.h"
#include "systems/light_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

#define MATERIAL_SHADER_NAME_STANDARD "Shader.MaterialStandard"
#define MATERIAL_SHADER_NAME_WATER "Shader.MaterialWater"
#define MATERIAL_SHADER_NAME_BLENDED "Shader.MaterialBlended"

// Textures
const u32 MAT_STANDARD_IDX_BASE_COLOR = 0;
const u32 MAT_STANDARD_IDX_NORMAL = 1;
const u32 MAT_STANDARD_IDX_METALLIC = 2;
const u32 MAT_STANDARD_IDX_ROUGHNESS = 3;
const u32 MAT_STANDARD_IDX_AO = 4;
const u32 MAT_STANDARD_IDX_MRA = 5;
const u32 MAT_STANDARD_IDX_EMISSIVE = 6;
const u32 MAT_STANDARD_IDX_SHADOW_MAP = 7;
const u32 MAT_STANDARD_IDX_IRRADIANCE_MAP = 8;

#define SHADOW_CASCADE_COUNT 4
#define MAX_POINT_LIGHTS 10

// Represents the data for a single instance of a material.
// This can be thought of as "per-draw" data
typedef struct material_instance_data
{
    // A unique id used for handle validation
    u64 unique_id;

    // A handle to the material to which this instance references
    bhandle material;

    // Multiplied by albedo/diffuse texture. Overrides the value set in the base material
    vec4 base_color;

    // Overrides the flags set in the base material
    bmaterial_flags flags;

    // Added to UV coords of vertex data
    vec3 uv_offset;
    // Multiplied against uv coords of vertex data
    vec3 uv_scale;

    // Shader draw id for per-draw uniforms
    u32 per_draw_id;

    // The generation of the material instance data. Incremented each time it is updated.
    // INVALID_ID_U16 means unloaded. Synced within the renderer backend as needed.
    // Can roll back around to 0
    u16 generation;
} material_instance_data;

// Represents a base material.
// This can be thought of as "per-group" data
typedef struct material_data
{
    bname name;
    /** @brief The material type. Ultimately determines what shader the material is rendered with */
    bmaterial_type type;
    /** @brief The material lighting model */
    bmaterial_model model;
    // A unique id used for handle validation
    u64 unique_id;

    vec4 base_color;
    bresource_texture* base_color_texture;

    vec3 normal;
    bresource_texture* normal_texture;

    f32 metallic;
    bresource_texture* metallic_texture;
    texture_channel metallic_texture_channel;

    f32 roughness;
    bresource_texture* roughness_texture;
    texture_channel roughness_texture_channel;

    f32 ao;
    bresource_texture* ao_texture;
    texture_channel ao_texture_channel;

    vec4 emissive;
    bresource_texture* emissive_texture;
    f32 emissive_texture_intensity;

    bresource_texture* refraction_texture;
    f32 refraction_scale;

    vec3 mra;
    /** @brief This is a combined texture holding metallic/roughness/ambient occlusion all in one texture */
    bresource_texture* mra_texture;

    // Base set of flags for the material. Copied to the material instance when created
    bmaterial_flags flags;

    // Added to UV coords of vertex data. Overridden by instance data
    vec3 uv_offset;
    // Multiplied against uv coords of vertex data. Overridden by instance data
    vec3 uv_scale;

    // Shader group id for per-group uniforms
    u32 group_id;

    // The generation of the material data. Incremented each time it is updated.
    // INVALID_ID_U16 means unloaded. Synced within the renderer backend as needed.
    // Can roll back around to 0
    u16 generation;
} material_data;

typedef enum material_standard_flag_bits
{
    MATERIAL_STANDARD_FLAG_USE_BASE_COLOR_TEX = 0x0001,
    MATERIAL_STANDARD_FLAG_USE_NORMAL_TEX = 0x0002,
    MATERIAL_STANDARD_FLAG_USE_METALLIC_TEX = 0x0004,
    MATERIAL_STANDARD_FLAG_USE_ROUGHNESS_TEX = 0x0008,
    MATERIAL_STANDARD_FLAG_USE_AO_TEX = 0x0010,
    MATERIAL_STANDARD_FLAG_USE_MRA_TEX = 0x0020,
    MATERIAL_STANDARD_FLAG_USE_EMISSIVE_TEX = 0x0040
} material_standard_flag_bits;

typedef u32 material_standard_flags;

typedef struct material_standard_shader_locations
{
    // Per frame
    u16 material_frame_ubo;
    u16 shadow_textures;
    u16 ibl_cube_textures;
    u16 shadow_sampler;
    u16 ibl_sampler;

    // Per group
    u16 material_textures;
    u16 material_samplers;
    u16 material_group_ubo;

    // Per draw
    u16 material_draw_ubo;
} material_standard_shader_locations;

// Per-frame UBO data -388 bytes
typedef struct material_standard_frame_uniform_data
{
    // Light space for shadow mapping. Per cascade
    mat4 directional_light_spaces[SHADOW_CASCADE_COUNT]; // 256 bytes
    mat4 projection;
    mat4 view;
    mat4 inv_view;
    vec3 view_position;
    f32 bias;
    vec3 inv_view_position;
    u32 render_mode;
    vec4 cascade_splits[SHADOW_CASCADE_COUNT];
    // HACK: Read this in from somewhere (or have global setter?);
    vec4 clipping_plane;
    u32 use_pcf;
} material_standard_frame_uniform_data;

// Per-group UBO data -656 bytes
typedef struct material_standard_group_uniform_data
{
    directional_light_data dir_light;            // 48 bytes
    point_light_data p_lights[MAX_POINT_LIGHTS]; // 48 bytes each
    i32 num_p_lights;
    // The material lighting model
    u32 model;
    // Base set of flags for the material. Copied to the material instance when created
    u32 flags;
    // Texture use flags
    u32 tex_flags;

    vec4 base_color;
    vec4 emissive;

    vec3 normal;
    f32 metallic;
    vec3 mra;
    f32 roughness;

    // Added to UV coords of vertex data. Overridden by instance data
    vec3 uv_offset;
    f32 ao;
    // Multiplied against uv coords of vertex data. Overridden by instance data
    vec3 uv_scale;
    f32 emissive_texture_intensity;

    f32 refraction_scale;
    f32 delta_time;
    f32 game_time;

    // Packed texture channels for various maps requiring it
    u32 texture_channels; // [metallic, roughness, ao, unused]
} material_standard_group_uniform_data;

// Per-draw UBO data - 84 bytes
typedef struct material_standard_draw_uniform_data
{
    mat4 model;
    vec4 clipping_plane;
    u32 view_index;
    u32 ibl_index;
} material_standard_draw_uniform_data;

/** @brief Holds internal state for per-frame data (i.e across all standard materials); */
typedef struct material_standard_frame_data
{
    // Light space for shadow mapping. Per cascade
    mat4 directional_light_spaces[SHADOW_CASCADE_COUNT];
    mat4 projection;
    mat4 view;
    mat4 inv_view;
    vec3 view_position;
    u32 render_mode;
    vec3 inv_view_position;
    u32 use_pcf;
    vec4 cascade_splits[SHADOW_CASCADE_COUNT];
    // HACK: Read this in from somewhere (or have global setter?);
    f32 bias;
    vec4 clipping_plane;
    u16 generation;
} material_standard_frame_data;

/** @brief The structure which holds state for the entire material system */
typedef struct material_system_state
{
    material_system_config config;

    // darray of materials, indexed by material bhandle resource index
    material_data* materials;
    // darray of material instances, indexed first by material bhandle index, then by instance bhandle index
    material_instance_data** instances;

    // A default material for each type of material
    bhandle default_standard_material;
    bhandle default_water_material;
    bhandle default_blended_material;
    material_standard_shader_locations standard_material_locations;
    material_standard_frame_data standard_frame_data;

    // Cached handles for various material types' shaders
    bhandle material_standard_shader;
    bhandle material_water_shader;
    bhandle material_blended_shader;

    // Keep a pointer to the renderer state for quick access
    struct renderer_system_state* renderer;
    struct texture_system_state* texture_system;
    struct bresource_system_state* resource_system;
} material_system_state;

// Holds data for a material instance request
typedef struct material_request_listener
{
    bhandle material_handle;
    bhandle* instance_handle;
    material_system_state* state;
} material_request_listener;

static b8 create_default_standard_material(material_system_state* state);
static b8 create_default_water_material(material_system_state* state);
static b8 create_default_blended_material(material_system_state* state);
static void on_material_system_dump(console_command_context context);
static bhandle get_shader_for_material_type(const material_system_state* state, bmaterial_type type);
static bhandle material_handle_create(material_system_state* state, bname name);
static bhandle material_instance_handle_create(material_system_state* state, bhandle material_handle);
static b8 material_create(material_system_state* state, bhandle material_handle, const bresource_material* typed_resource);
static void material_destroy(material_system_state* state, bhandle* material_handle);
static b8 material_instance_create(material_system_state* state, bhandle base_material, bhandle* out_instance_handle);
static void material_instance_destroy(material_system_state* state, bhandle base_material, bhandle* instance_handle);
static void material_resource_loaded(bresource* resource, void* listener);
static material_instance default_material_instance_get(material_system_state* state, bhandle base_material, const char* name_str);
static material_instance_data* get_instance_data(material_system_state* state, material_instance instance);
static void default_standard_material_locations_get(material_system_state* state);
static void increment_generation(u16* generation);

b8 material_system_initialize(u64* memory_requirement, material_system_state* state, const material_system_config* config)
{
    material_system_config* typed_config = (material_system_config*)config;
    if (typed_config->max_material_count == 0)
    {
        BFATAL("material_system_initialize - config.max_material_count must be > 0");
        return false;
    }

    // Block of memory will contain state structure, then block for array, then block for hashtable
    *memory_requirement = sizeof(material_system_state);
    if (!state)
        return true;

    // Keep a pointer to the renderer system state for quick access
    const engine_system_states* states = engine_systems_get();
    state->renderer = states->renderer_system;
    state->resource_system = states->bresource_state;
    state->texture_system = states->texture_system;

    state->config = *typed_config;

    state->materials = darray_create(material_data);
    // An array for each material will be created when a material is created
    state->instances = darray_create(material_instance_data*);

    // Get default material shaders
    state->material_standard_shader = shader_system_get(bname_create(MATERIAL_SHADER_NAME_STANDARD));
    default_standard_material_locations_get(state);
    // Setup per-frame data for the standard shader
    state->standard_frame_data.projection = mat4_perspective(deg_to_rad(45.0f), 720.0f / 1280.0f, 0.01f, 1000.0f);
    state->standard_frame_data.inv_view = mat4_look_at(vec3_zero(), vec3_forward(), vec3_up());
    state->standard_frame_data.inv_view_position = vec3_zero();
    state->standard_frame_data.view = mat4_inverse(state->standard_frame_data.inv_view);
    state->standard_frame_data.view_position = vec3_zero();
    state->standard_frame_data.render_mode = 0;
    for (u32 i = 0; i < SHADOW_CASCADE_COUNT; ++i)
    {
        state->standard_frame_data.cascade_splits[i] = vec4_zero();
        state->standard_frame_data.directional_light_spaces[i] = mat4_identity();
    }
    state->standard_frame_data.use_pcf = 1;
    state->standard_frame_data.bias = 0.0005f;
    state->standard_frame_data.clipping_plane = vec4_zero();

    state->material_water_shader = shader_system_get(bname_create(MATERIAL_SHADER_NAME_WATER));
    state->material_blended_shader = shader_system_get(bname_create(MATERIAL_SHADER_NAME_BLENDED));

    // Load up some default materials
    if (!create_default_standard_material(state))
    {
        BFATAL("Failed to create default standard material. Application cannot continue");
        return false;
    }

    if (!create_default_water_material(state))
    {
        BFATAL("Failed to create default blended material. Application cannot continue");
        return false;
    }

    if (!create_default_blended_material(state))
    {
        BFATAL("Failed to create default blended material. Application cannot continue");
        return false;
    }

    // Register a console command to dump list of materials/references
    console_command_register("material_system_dump", 0, on_material_system_dump);

    return true;
}

void material_system_shutdown(struct material_system_state* state)
{
    if (state)
    {
        // Destroy default materials
        material_destroy(state, &state->default_standard_material);
        material_destroy(state, &state->default_water_material);
        material_destroy(state, &state->default_blended_material);

        // Release shaders for the default materials
        shader_system_destroy(&state->material_standard_shader);
        shader_system_destroy(&state->material_water_shader);
        shader_system_destroy(&state->material_blended_shader);
    }
}

b8 material_system_acquire(material_system_state* state, bname name, material_instance* out_instance)
{
    BASSERT_MSG(out_instance, "out_instance is required");

    u32 material_count = darray_length(state->materials);
    for (u32 i = 0; i < material_count; ++i)
    {
        material_data* material = &state->materials[i];
        if (material->name == name)
        {
            // Material exists, create an instance and boot
            out_instance->material = bhandle_create_with_u64_identifier(i, material->unique_id);

            // Request instance and set handle
            b8 instance_result = material_instance_create(state, out_instance->material, &out_instance->instance);
            if (!instance_result)
                BERROR("Failed to create material instance during new material creation");
            return instance_result;
        }
    }

    // Material is not yet loaded, request it
    // Setup a listener
    material_request_listener* listener = BALLOC_TYPE(material_request_listener, MEMORY_TAG_MATERIAL_INSTANCE);
    listener->state = state;
    listener->material_handle = material_handle_create(state, name);
    listener->instance_handle = &out_instance->instance;

    // Request the resource
    bresource_material_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_MATERIAL;
    request.base.user_callback = material_resource_loaded;
    request.base.listener_inst = listener;
    bresource* r = bresource_system_request(state->resource_system, name, (bresource_request_info*)&request);
    return r != 0;
}

void material_system_release(material_system_state* state, material_instance* instance)
{
    if (!state)
        return;

    // Getting the material instance data successfully performs all handle checks for the material and instance. This means it's safe to destroy
    if (get_instance_data(state, *instance))
    {
        material_instance_destroy(state, instance->material, &instance->instance);
        // Invalidate the material handle in the instance pointer as well
        bhandle_invalidate(&instance->material);
    }
}

b8 material_system_prepare_frame(material_system_state* state, frame_data* p_frame_data)
{
    if (!state)
        return false;

    // Standard shader type
    {
        bhandle shader = state->material_standard_shader;

        // Setup frame data UBO structure to send over
        material_standard_frame_uniform_data frame_ubo = {0};
        frame_ubo.projection = state->standard_frame_data.projection;
        frame_ubo.view = state->standard_frame_data.view;
        frame_ubo.inv_view = state->standard_frame_data.inv_view;
        frame_ubo.view_position = state->standard_frame_data.view_position;
        frame_ubo.inv_view_position = state->standard_frame_data.inv_view_position;
        frame_ubo.bias = state->standard_frame_data.bias;
        frame_ubo.render_mode = state->standard_frame_data.render_mode;
        frame_ubo.clipping_plane = state->standard_frame_data.clipping_plane;
        frame_ubo.use_pcf = state->standard_frame_data.use_pcf;
        for (u8 i = 0; i < MAX_SHADOW_CASCADE_COUNT; ++i)
        {
            frame_ubo.cascade_splits[i] = state->standard_frame_data.cascade_splits[i];
            frame_ubo.directional_light_spaces[i] = state->standard_frame_data.directional_light_spaces[i];
        }
        state->standard_frame_data.generation++;

        if (!shader_system_bind_frame(shader))
        {
            BERROR("Failed to bind frame frequency for standard material shader");
            return false;
        }

        // Set the whole thing at once
        shader_system_uniform_set_by_location(shader, state->standard_material_locations.material_frame_ubo, &frame_ubo);

        // Apply/upload them to the GPU
        if (!shader_system_apply_per_frame(shader, state->standard_frame_data.generation))
        {
            BERROR("Failed to apply per-frame uniforms");
            return false;
        }
    }

    // TODO: Water

    // TODO: Blended

    return true;
}

b8 material_system_apply(material_system_state* state, bhandle material, frame_data* p_frame_data)
{
    if (!state)
        return false;

    material_data* base_material = &state->materials[material.handle_index];

    bhandle shader;

    switch (base_material->type)
    {
    default:
    case BMATERIAL_TYPE_UNKNOWN:
        BASSERT_MSG(false, "Unknown shader type cannot be applied");
        return false;
    case BMATERIAL_TYPE_STANDARD:
    {
        shader = state->material_standard_shader;

        // per-group - ensure this is done once per frame per material

        // bind per-group
        if (!shader_system_bind_group(material, base_material->group_id))
        {
            BERROR("Failed to bind material shader group");
            return false;
        }

        // Setup frame data UBO structure to send over
        material_standard_group_uniform_data group_ubo = {0};
        group_ubo.flags = base_material->flags;
        group_ubo.tex_flags = 0;

        // Directional light
        directional_light* dir_light = light_system_directional_light_get();
        if (dir_light)
        {
            group_ubo.dir_light = dir_light->data;
        }
        else
        {
            BERROR("Failed to bind material shader group");
            return false;
            bzero_memory(&group_ubo.dir_light, sizeof(directional_light_data));
        }
        // Point lights
        group_ubo.num_p_lights = BMIN(light_system_point_light_count(), MAX_POINT_LIGHTS);
        if (group_ubo.num_p_lights)
        {
            point_light p_lights[MAX_POINT_LIGHTS];
            bzero_memory(p_lights, sizeof(point_light) * MAX_POINT_LIGHTS);

            light_system_point_lights_get(p_lights);

            for (u32 i = 0; i < group_ubo.num_p_lights; ++i)
                group_ubo.p_lights[i] = p_lights[i].data;
        }

        // Inputs - Bind the texture if used
        // Base color
        if (base_material->base_color_texture)
        {
            group_ubo.tex_flags = FLAG_SET(group_ubo.tex_flags, MATERIAL_STANDARD_FLAG_USE_BASE_COLOR_TEX, true);
            shader_system_uniform_set_by_location_arrayed(shader, state->standard_material_locations.material_textures, MAT_STANDARD_IDX_BASE_COLOR, &base_material->base_color_texture);
        }
        else
        {
            group_ubo.base_color = base_material->base_color;
        }

        // Normal
        if (FLAG_GET(base_material->flags, BMATERIAL_FLAG_NORMAL_ENABLED_BIT))
        {
            if (base_material->normal_texture) {
                group_ubo.tex_flags = FLAG_SET(group_ubo.tex_flags, MATERIAL_STANDARD_FLAG_USE_NORMAL_TEX, true);
                shader_system_uniform_set_by_location_arrayed(shader, state->standard_material_locations.material_textures, MAT_STANDARD_IDX_NORMAL, &base_material->normal_texture);
            }
            else
            {
                group_ubo.normal = base_material->normal;
            }
        }

        // MRA
        b8 mra_enabled = FLAG_GET(base_material->flags, BMATERIAL_FLAG_MRA_ENABLED_BIT);
        if (mra_enabled)
        {
            if (base_material->mra_texture)
            {
                group_ubo.tex_flags = FLAG_SET(group_ubo.tex_flags, MATERIAL_STANDARD_FLAG_USE_MRA_TEX, true);
                shader_system_uniform_set_by_location_arrayed(shader, state->standard_material_locations.material_textures, MAT_STANDARD_IDX_MRA, &base_material->mra_texture);
            }
            else
            {
                group_ubo.mra = base_material->mra;
            }
        }
        else
        {
            // If not using MRA, then do these:

            // Metallic
            if (base_material->metallic_texture)
            {
                group_ubo.tex_flags = FLAG_SET(group_ubo.tex_flags, MATERIAL_STANDARD_FLAG_USE_METALLIC_TEX, true);
                shader_system_uniform_set_by_location_arrayed(shader, state->standard_material_locations.material_textures, MAT_STANDARD_IDX_METALLIC, &base_material->metallic_texture);
            }
            else
            {
                group_ubo.metallic = base_material->metallic;
            }

            // Roughness
            if (base_material->roughness_texture)
            {
                group_ubo.tex_flags = FLAG_SET(group_ubo.tex_flags, MATERIAL_STANDARD_FLAG_USE_ROUGHNESS_TEX, true);
                shader_system_uniform_set_by_location_arrayed(shader, state->standard_material_locations.material_textures, MAT_STANDARD_IDX_ROUGHNESS, &base_material->roughness_texture);
            }
            else
            {
                group_ubo.roughness = base_material->roughness;
            }
            
            // AO
            if (base_material->ao_texture && FLAG_GET(base_material->flags, BMATERIAL_FLAG_AO_ENABLED_BIT))
            {
                group_ubo.tex_flags = FLAG_SET(group_ubo.tex_flags, MATERIAL_STANDARD_FLAG_USE_AO_TEX, true);
                shader_system_uniform_set_by_location_arrayed(shader, state->standard_material_locations.material_textures, MAT_STANDARD_IDX_NORMAL, &base_material->ao_texture);
            }
            else
            {
                group_ubo.ao = base_material->ao;
            }

            // Pack source channels. [Metallic, roughness, ao, unused]
            group_ubo.texture_channels = pack_u8_into_u32(base_material->metallic_texture_channel, base_material->roughness_texture_channel, base_material->ao_texture_channel, 0);
        }

        // Emissive
        if (base_material->emissive_texture && FLAG_GET(base_material->flags, BMATERIAL_FLAG_EMISSIVE_ENABLED_BIT))
        {
            group_ubo.tex_flags = FLAG_SET(group_ubo.tex_flags, MATERIAL_STANDARD_FLAG_USE_EMISSIVE_TEX, true);
            shader_system_uniform_set_by_location_arrayed(shader, state->standard_material_locations.material_textures, MAT_STANDARD_IDX_EMISSIVE, &base_material->emissive_texture);
        }
        else
        {
            group_ubo.emissive = base_material->emissive;
        }
        
        // Set the whole thing at once
        shader_system_uniform_set_by_location(shader, state->standard_material_locations.material_group_ubo, &group_ubo);

        // Apply/upload them to the GPU
        shader_system_apply_per_group(shader, base_material->generation);
    }
        return true;
    case BMATERIAL_TYPE_WATER:
        shader = state->material_water_shader;
        return false;
    case BMATERIAL_TYPE_BLENDED:
        shader = state->material_blended_shader;
        return false;
    case BMATERIAL_TYPE_CUSTOM:
        BASSERT_MSG(false, "Not yet implemented!");
        return false;
    }
}

b8 material_system_apply_instance(material_system_state* state, const material_instance* instance, frame_data* p_frame_data)
{
    if (!state)
        return false;

    material_instance_data* instance_data = get_instance_data(state, *instance);
    if (!instance_data)
        return false;

    material_data* base_material = &state->materials[instance->material.handle_index];

    bhandle shader;

    switch (base_material->type)
    {
    default:
    case BMATERIAL_TYPE_UNKNOWN:
        BASSERT_MSG(false, "Unknown shader type cannot be applied");
        return false;
    case BMATERIAL_TYPE_STANDARD: {
        shader = state->material_standard_shader;

        // per-draw - this gets run every time apply is called
        // bind per-draw
        if (!shader_system_bind_draw_id(state->material_standard_shader, instance_data->per_draw_id))
        {
            BERROR("Failed to bind material shader draw id");
            return false;
        }
        // update uniforms if dirty
        // TODO: Dirty check
        material_standard_draw_uniform_data draw_ubo = {0};
        draw_ubo.clipping_plane = vec4_zero(); //  FIXME: This should probably be defined per reflective surface used

        draw_ubo.model = mat4_identity();
        draw_ubo.ibl_index = 0;  // TODO: Should be provided externally
        draw_ubo.view_index = 0; // FIXME: This won't render reflections properly until we pass this in

        // Set the whole thing at once
        shader_system_uniform_set_by_location(shader, state->standard_material_locations.material_draw_ubo, &draw_ubo);

        // apply per-draw
        shader_system_apply_per_draw(state->material_standard_shader, instance_data->generation);
    }
        return true;
    case BMATERIAL_TYPE_WATER:
        shader = state->material_water_shader;
        return false;
    case BMATERIAL_TYPE_BLENDED:
        shader = state->material_blended_shader;
        return false;
    case BMATERIAL_TYPE_CUSTOM:
        BASSERT_MSG(false, "Not yet implemented!");
        return false;
    }
}

b8 material_instance_flag_set(struct material_system_state* state, material_instance instance, bmaterial_flag_bits flag, b8 value)
{
    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    data->flags = FLAG_SET(data->flags, flag, value);

    return true;
}

b8 material_instance_flag_get(struct material_system_state* state, material_instance instance, bmaterial_flag_bits flag)
{
    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    return FLAG_GET(data->flags, flag);
}

b8 material_instance_base_color_get(struct material_system_state* state, material_instance instance, vec4* out_value)
{
    if (!out_value)
        return false;

    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    *out_value = data->base_color;
    return true;
}

b8 material_instance_base_color_set(struct material_system_state* state, material_instance instance, vec4 value)
{
    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    data->base_color = value;
    increment_generation(&data->generation);
    return true;
}

b8 material_instance_uv_offset_get(struct material_system_state* state, material_instance instance, vec3* out_value)
{
    if (!out_value)
        return false;

    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    *out_value = data->uv_offset;
    return true;
}

b8 material_instance_uv_offset_set(struct material_system_state* state, material_instance instance, vec3 value)
{
    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    data->uv_offset = value;
    increment_generation(&data->generation);
    return true;
}

b8 material_instance_uv_scale_get(struct material_system_state* state, material_instance instance, vec3* out_value)
{
    if (!out_value)
        return false;

    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    *out_value = data->uv_scale;
    return true;
}

b8 material_instance_uv_scale_set(struct material_system_state* state, material_instance instance, vec3 value)
{
    material_instance_data* data = get_instance_data(state, instance);
    if (!data)
        return false;

    data->uv_offset = value;
    increment_generation(&data->generation);
    return true;
}

material_instance material_system_get_default_standard(material_system_state* state)
{
    return default_material_instance_get(state, state->default_standard_material, "standard");
}

material_instance material_system_get_default_water(material_system_state* state)
{
    return default_material_instance_get(state, state->default_water_material, "water");
}

material_instance material_system_get_default_blended(material_system_state* state)
{
    return default_material_instance_get(state, state->default_blended_material, "blended");
}

void material_system_dump(material_system_state* state)
{
    u32 material_count = darray_length(state->materials);
    for (u32 i = 0; i < material_count; ++i)
    {
        material_data* m = &state->materials[i];
        // Skip "free" slots
        if (m->unique_id == INVALID_ID_U64)
            continue;

        material_instance_data* instance_array = state->instances[i];
        // Get a count of active instances
        u32 instance_count = darray_length(instance_array);
        u32 active_instance_count = 0;
        for (u32 j = 0; j < instance_count; ++j)
        {
            if (instance_array[j].unique_id != INVALID_ID_U64)
                active_instance_count++;
        }

        BTRACE("Material name: '%s', active instance count = %u", bname_string_get(m->name), active_instance_count);
    }
}

static b8 create_default_standard_material(material_system_state* state)
{
    bname material_name = bname_create(MATERIAL_DEFAULT_NAME_STANDARD);

    // Create a fake material "asset" that can be serialized into a string
    basset_material asset = {0};
    asset.base.name = material_name;
    asset.base.type = BASSET_TYPE_MATERIAL;
    asset.type = BMATERIAL_TYPE_STANDARD;
    asset.has_transparency = false;
    asset.double_sided = false;
    asset.recieves_shadow = true;
    asset.casts_shadow = true;
    asset.use_vertex_color_as_base_color = false;
    asset.base_color = vec4_one(); // white
    asset.normal = vec3_create(0.0f, 0.0f, 1.0f);
    asset.normal_enabled = true;
    asset.mra = vec3_create(0.0f, 0.5f, 1.0f);
    asset.use_mra = true;

    // Setup a listener
    material_request_listener* listener = BALLOC_TYPE(material_request_listener, MEMORY_TAG_MATERIAL_INSTANCE);
    listener->state = state;
    listener->material_handle = material_handle_create(state, material_name);
    listener->instance_handle = 0; // NOTE: creation of default materials does not immediately need an instance

    bresource_material_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_MATERIAL;
    request.base.listener_inst = listener;
    request.base.user_callback = material_resource_loaded;
    // The material source is serialized into a string
    request.material_source_text = basset_material_serialize((basset*)&asset);

    if (!bresource_system_request(state->resource_system, bname_create("default"), (bresource_request_info*)&request))
    {
        BERROR("Resource request for default standard material failed. See logs for details");
        return false;
    }

    return true;
}

static b8 create_default_water_material(material_system_state* state)
{
    // TODO: ...
    return true;
}

static b8 create_default_blended_material(material_system_state* state)
{
    bresource_material_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_MATERIAL;
    request.material_source_text = "\
version = 3\
type = \"multi\"\
\
materials = [\
    \"default\"\
    \"default\"\
    \"default\"\
    \"default\"\
]";

    return true;
}

static void on_material_system_dump(console_command_context context)
{
    material_system_dump(engine_systems_get()->material_system);
}

static bhandle get_shader_for_material_type(const material_system_state* state, bmaterial_type type)
{
    switch (type)
    {
    default:
    case BMATERIAL_TYPE_UNKNOWN:
        BERROR("Cannot create a material using an 'unknown' material type");
        return bhandle_invalid();
    case BMATERIAL_TYPE_STANDARD:
        return state->material_standard_shader;
        break;
    case BMATERIAL_TYPE_WATER:
        return state->material_water_shader;
        break;
    case BMATERIAL_TYPE_BLENDED:
        return state->material_blended_shader;
        break;
    case BMATERIAL_TYPE_CUSTOM:
        BASSERT_MSG(false, "Not yet implemented!");
        return bhandle_invalid();
    }
}

static bhandle material_handle_create(material_system_state* state, bname name)
{
    u32 resource_index = INVALID_ID;

    // Attempt to find a free "slot", or create a new entry if there isn't one
    u32 material_count = darray_length(state->materials);
    for (u32 i = 0; i < material_count; ++i)
    {
        if (state->materials[i].unique_id == INVALID_ID_U64)
        {
            // free slot. An array should already exists for instances here
            resource_index = i;
            break;
        }
    }
    if (resource_index == INVALID_ID)
    {
        resource_index = material_count;
        darray_push(state->materials, (material_data){0});
        // This also means a new entry needs to be created at this index for instances
        material_instance_data* new_inst_array = darray_create(material_instance_data);
        new_inst_array->unique_id = INVALID_ID_U64;
        darray_push(state->instances, new_inst_array);
    }

    material_data* material = &state->materials[resource_index];

    // Setup a handle first
    bhandle handle = bhandle_create(resource_index);
    material->unique_id = handle.unique_id.uniqueid;
    material->name = name;

    return handle;
}

static bhandle material_instance_handle_create(material_system_state* state, bhandle material_handle)
{
    u32 instance_index = INVALID_ID;

    // Attempt to find a free "slot", or create a new entry if there isn't one
    u32 instance_count = darray_length(state->instances[material_handle.handle_index]);
    for (u32 i = 0; i < instance_count; ++i)
    {
        if (state->instances[material_handle.handle_index][i].unique_id == INVALID_ID_U64)
        {
            // free slot. An array should already exists for instances here
            instance_index = i;
            break;
        }
    }
    if (instance_index == INVALID_ID)
    {
        instance_index = instance_count;
        darray_push(state->instances[material_handle.handle_index], (material_instance_data){0});
    }

    material_instance_data* inst = &state->instances[material_handle.handle_index][instance_index];

    // Setup a handle first
    bhandle handle = bhandle_create(instance_index);
    inst->unique_id = handle.unique_id.uniqueid;
    inst->material = material_handle;

    return handle;
}

static b8 material_create(material_system_state* state, bhandle material_handle, const bresource_material* typed_resource)
{
    material_data* material = &state->materials[material_handle.handle_index];

    // Validate the material type and model
    material->type = typed_resource->type;
    material->model = typed_resource->model;

    // Select shader
    bhandle material_shader = get_shader_for_material_type(state, material->type);
    if (bhandle_is_invalid(material_shader))
    {
        // TODO: invalidate handle/entry?
        return false;
    }
    
    // Base color map or value
    if (typed_resource->base_color_map.resource_name)
    {
        material->base_color_texture = texture_system_request(typed_resource->base_color_map.resource_name, typed_resource->base_color_map.package_name, 0, 0);
    }
    else
    {
        material->base_color = typed_resource->base_color;
    }

    // Normal map
    if (typed_resource->normal_map.resource_name)
        material->normal_texture = texture_system_request(typed_resource->normal_map.resource_name, typed_resource->normal_map.package_name, 0, 0);
    material->flags |= typed_resource->normal_enabled ? BMATERIAL_FLAG_NORMAL_ENABLED_BIT : 0;

    // Metallic map or value
    if (typed_resource->metallic_map.resource_name)
    {
        material->metallic_texture = texture_system_request(typed_resource->metallic_map.resource_name, typed_resource->metallic_map.package_name, 0, 0);
        material->metallic_texture_channel = typed_resource->metallic_map.channel;
    }
    else
    {
        material->metallic = typed_resource->metallic;
    }

    // Roughness map or value
    if (typed_resource->roughness_map.resource_name)
    {
        material->roughness_texture = texture_system_request(typed_resource->roughness_map.resource_name, typed_resource->roughness_map.package_name, 0, 0);
        material->roughness_texture_channel = typed_resource->roughness_map.channel;
    }
    else
    {
        material->roughness = typed_resource->roughness;
    }

    // Ambient occlusion map or value
    if (typed_resource->ambient_occlusion_map.resource_name)
    {
        material->ao_texture = texture_system_request(typed_resource->ambient_occlusion_map.resource_name, typed_resource->ambient_occlusion_map.package_name, 0, 0);
        material->ao_texture_channel = typed_resource->ambient_occlusion_map.channel;
    }
    else
    {
        material->ao = typed_resource->ambient_occlusion;
    }
    material->flags |= typed_resource->ambient_occlusion_enabled ? BMATERIAL_FLAG_AO_ENABLED_BIT : 0;

    // MRA (combined metallic/roughness/ao) map or value
    if (typed_resource->mra_map.resource_name)
    {
        material->mra_texture = texture_system_request(typed_resource->mra_map.resource_name, typed_resource->mra_map.package_name, 0, 0);
    }
    else
    {
        material->mra = typed_resource->mra;
    }
    material->flags |= typed_resource->use_mra ? BMATERIAL_FLAG_MRA_ENABLED_BIT : 0;

    // Emissive map or value
    if (typed_resource->emissive_map.resource_name)
    {
        material->emissive_texture = texture_system_request(typed_resource->emissive_map.resource_name, typed_resource->emissive_map.package_name, 0, 0);
    }
    else
    {
        material->emissive = typed_resource->emissive;
    }
    material->flags |= typed_resource->emissive_enabled ? BMATERIAL_FLAG_EMISSIVE_ENABLED_BIT : 0;

    // Set remaining flags
    material->flags |= typed_resource->has_transparency ? BMATERIAL_FLAG_HAS_TRANSPARENCY_BIT : 0;
    material->flags |= typed_resource->double_sided ? BMATERIAL_FLAG_DOUBLE_SIDED_BIT : 0;
    material->flags |= typed_resource->recieves_shadow ? BMATERIAL_FLAG_RECIEVES_SHADOW_BIT : 0;
    material->flags |= typed_resource->casts_shadow ? BMATERIAL_FLAG_CASTS_SHADOW_BIT : 0;
    material->flags |= typed_resource->use_vertex_color_as_base_color ? BMATERIAL_FLAG_USE_VERTEX_COLOR_AS_BASE_COLOR_BIT : 0;

    // Create a group for the material
    if (!shader_system_shader_group_acquire(material_shader, &material->group_id))
    {
        BERROR("Failed to acquire shader group while creating material. See logs for details");
        // TODO: destroy/release
        return false;
    }

    // TODO: Custom samplers

    return true;
}

static void material_destroy(material_system_state* state, bhandle* material_handle)
{
    if (bhandle_is_invalid(*material_handle) || bhandle_is_stale(*material_handle, state->materials[material_handle->handle_index].unique_id))
    {
        BWARN("Attempting to release material that has an invalid or stale handle");
        return;
    }

    material_data* material = &state->materials[material_handle->handle_index];

    // Select shader
    bhandle material_shader = get_shader_for_material_type(state, material->type);
    if (bhandle_is_invalid(material_shader))
    {
        BWARN("Attempting to release material that had an invalid shader");
        return;
    }

    // Release texture resources/references
    if (material->base_color_texture)
    {
        texture_system_release_resource(material->base_color_texture);
    }
    if (material->normal_texture)
    {
        texture_system_release_resource(material->normal_texture);
    }
    if (material->metallic_texture)
    {
        texture_system_release_resource(material->metallic_texture);
    }
    if (material->roughness_texture)
    {
        texture_system_release_resource(material->roughness_texture);
    }
    if (material->ao_texture)
    {
        texture_system_release_resource(material->ao_texture);
    }
    if (material->mra_texture)
    {
        texture_system_release_resource(material->mra_texture);
    }
    if (material->emissive_texture)
    {
        texture_system_release_resource(material->emissive_texture);
    }

    // Release the group for the material
    if (!shader_system_shader_group_release(material_shader, material->group_id))
        BWARN("Failed to release shader group while creating material. See logs for details");

    // TODO: Custom samplers

    // Destroy instances
    u32 instance_count = darray_length(state->instances[material_handle->handle_index]);
    for (u32 i = 0; i < instance_count; ++i)
    {
        material_instance_data* inst = &state->instances[material_handle->handle_index][i];
        if (inst->unique_id != INVALID_ID_U64)
        {
            bhandle temp_handle = bhandle_create_with_u64_identifier(i, inst->unique_id);
            material_instance_destroy(state, *material_handle, &temp_handle);
        }
    }

    bzero_memory(material, sizeof(material_data));

    // Mark the material slot as free for another material to be loaded
    material->unique_id = INVALID_ID_U64;
    material->group_id = INVALID_ID;

    bhandle_invalidate(material_handle);
}

static b8 material_instance_create(material_system_state* state, bhandle base_material, bhandle* out_instance_handle)
{
    *out_instance_handle = material_instance_handle_create(state, base_material);
    if (bhandle_is_invalid(*out_instance_handle))
    {
        BERROR("Failed to create material instance handle. Instance will not be created");
        return false;
    }

    material_data* material = &state->materials[base_material.handle_index];
    material_instance_data* inst = &state->instances[base_material.handle_index][out_instance_handle->handle_index];

    // Get per-draw resources for the instance
    if (!renderer_shader_per_draw_resources_acquire(state->renderer, get_shader_for_material_type(state, material->type), &inst->per_draw_id))
    {
        BERROR("Failed to create per-draw resources for a material instance. Instance creation failed");
        return false;
    }

    // Take a copy of the base material properties
    inst->flags = material->flags;
    inst->uv_scale = material->uv_scale;
    inst->uv_offset = material->uv_offset;
    inst->base_color = material->base_color;

    // New instances are always dirty
    increment_generation(&inst->generation);

    return true;
}

static void material_instance_destroy(material_system_state* state, bhandle base_material, bhandle* instance_handle)
{
    material_data* material = &state->materials[base_material.handle_index];
    material_instance_data* inst = &state->instances[base_material.handle_index][instance_handle->handle_index];
    if (bhandle_is_invalid(*instance_handle) || bhandle_is_stale(*instance_handle, state->instances[base_material.handle_index][instance_handle->handle_index].unique_id))
    {
        BWARN("Tried to destroy a material instance whose handle is either invalid or stale. Nothing will be done");
        return;
    }

    // Release per-draw resources for the instance
    renderer_shader_per_draw_resources_release(state->renderer, get_shader_for_material_type(state, material->type), inst->per_draw_id);

    bzero_memory(inst, sizeof(material_instance_data));

    // Make sure to invalidate the entry
    inst->unique_id = INVALID_ID_U64;
    inst->per_draw_id = INVALID_ID;

    // Invalidate the handle too
    bhandle_invalidate(instance_handle);
}

static void material_resource_loaded(bresource* resource, void* listener)
{
    bresource_material* typed_resource = (bresource_material*)resource;
    material_request_listener* listener_inst = (material_request_listener*)listener;
    material_system_state* state = listener_inst->state;

    // Create the base material
    if (!material_create(state, listener_inst->material_handle, typed_resource))
    {
        BERROR("Failed to create material. See logs for details");
        return;
    }

    // Create an instance of it if one is required
    if (listener_inst->instance_handle)
    {
        if (!material_instance_create(state, listener_inst->material_handle, listener_inst->instance_handle))
            BERROR("Failed to create material instance during new material creation");
    }
}

static material_instance default_material_instance_get(material_system_state* state, bhandle base_material, const char* name_str)
{
    material_instance instance = {0};
    instance.material = base_material;

    // Get an instance of it
    if (!material_instance_create(state, instance.material, &instance.instance))
    {
        // Fatal here because if this happens on a default material, something is seriously borked
        BFATAL("Failed to obtain an instance of the default %s material", name_str);

        // Invalidate the handles
        bhandle_invalidate(&instance.material);
        bhandle_invalidate(&instance.instance);
    }

    return instance;
}

static material_instance_data* get_instance_data(material_system_state* state, material_instance instance)
{
    if (!state)
        return 0;

    // Verify handles first
    if (bhandle_is_invalid(instance.material) || bhandle_is_invalid(instance.instance))
    {
        BWARN("Attempted to get material instance with an invalid base material or instance handle. Nothing to do");
        return 0;
    }

    if (bhandle_is_stale(instance.material, state->materials[instance.material.handle_index].unique_id))
    {
        BWARN("Attempted to get material instance using a stale material handle. Nothing will be done");
        return 0;
    }

    if (bhandle_is_stale(instance.material, state->instances[instance.material.handle_index][instance.instance.handle_index].unique_id))
    {
        BWARN("Attempted to get material instance using a stale material instance handle. Nothing will be done");
        return 0;
    }

    return &state->instances[instance.material.handle_index][instance.instance.handle_index];
}

static void default_standard_material_locations_get(material_system_state* state)
{
    // Save off the shader's uniform locations
    
    // Per frame
    state->standard_material_locations.material_frame_ubo = shader_system_uniform_location(state->material_standard_shader, bname_create("material_frame_ubo"));
    state->standard_material_locations.shadow_textures = shader_system_uniform_location(state->material_standard_shader, bname_create("shadow_textures"));
    state->standard_material_locations.ibl_cube_textures = shader_system_uniform_location(state->material_standard_shader, bname_create("ibl_cube_textures"));
    state->standard_material_locations.shadow_sampler = shader_system_uniform_location(state->material_standard_shader, bname_create("shadow_sampler"));
    state->standard_material_locations.ibl_sampler = shader_system_uniform_location(state->material_standard_shader, bname_create("ibl_sampler"));

    // Per group
    state->standard_material_locations.material_textures = shader_system_uniform_location(state->material_standard_shader, bname_create("material_textures"));
    state->standard_material_locations.material_samplers = shader_system_uniform_location(state->material_standard_shader, bname_create("material_samplers"));
    state->standard_material_locations.material_group_ubo = shader_system_uniform_location(state->material_standard_shader, bname_create("material_group_ubo"));

    // Per draw.
    state->standard_material_locations.material_draw_ubo = shader_system_uniform_location(state->material_standard_shader, bname_create("material_draw_ubo"));
}

static void increment_generation(u16* generation)
{
    (*generation)++;
    // Roll over to ensure a valid generation
    if ((*generation) == INVALID_ID_U16)
        (*generation) = 0;
}
