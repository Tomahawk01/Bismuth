#include "material_system.h"

#include "containers/darray.h"
#include "containers/hashtable.h"
#include "core/console.h"
#include "core/engine.h"
#include "defines.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.h"
#include "resources/resource_types.h"
#include "strings/bstring.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

#ifndef PBR_MAP_COUNT
#define PBR_MAP_COUNT 5
#endif

#define MAX_SHADOW_CASCADE_COUNT 4

// Samplers
const u32 SAMP_ALBEDO = 0;
const u32 SAMP_NORMAL = 1;
const u32 SAMP_COMBINED = 2;
const u32 SAMP_SHADOW_MAP = 3;
const u32 SAMP_IRRADIANCE_MAP = 4;

// The number of textures for a PBR material
#define PBR_MATERIAL_TEXTURE_COUNT 3

// Terrain materials are now all loaded into a single array texture
const u32 SAMP_TERRAIN_MATERIAL_ARRAY_MAP = 0;
const u32 SAMP_TERRAIN_SHADOW_MAP = 1 + SAMP_TERRAIN_MATERIAL_ARRAY_MAP;
const u32 SAMP_TERRAIN_IRRADIANCE_MAP = 1 + SAMP_TERRAIN_SHADOW_MAP;
// 1 array map for terrain materials, 1 for shadow map, 1 for irradiance map
const u32 TERRAIN_SAMP_COUNT = 3;

#define MAX_TERRAIN_MATERIAL_COUNT 4

typedef struct material_system_state
{
    material_system_config config;

    material default_pbr_material;
    material default_terrain_material;

    // Array of registered materials
    material* registered_materials;

    // Hashtable for material lookups
    hashtable registered_material_table;

    u32 terrain_shader_id;
    shader* terrain_shader;

    u32 pbr_shader_id;
    shader* pbr_shader;

    // Current irradiance cubemap texture to be used
    texture* irradiance_cube_texture;

    // The current shadow texture to be used for the next draw
    texture* shadow_texture;

    mat4 directional_light_space[MAX_SHADOW_CASCADE_COUNT];

    // Keep a pointer to the renderer state for quick access
    struct renderer_system_state* renderer;
} material_system_state;

typedef struct material_reference
{
    u64 reference_count;
    u32 handle;
    b8 auto_release;
} material_reference;

static material_system_state* state_ptr = 0;

static b8 create_default_pbr_material(material_system_state* state);
static b8 create_default_terrain_material(material_system_state* state);
static b8 load_material(material_config* config, material* m);
static void destroy_material(material* m);

static b8 assign_map(texture_map* map, const material_map* config, const char* material_name, texture* default_tex);
static void on_material_system_dump(console_command_context context);

b8 material_system_initialize(u64* memory_requirement, void* state, void* config)
{
    material_system_config* typed_config = (material_system_config*)config;
    if (typed_config->max_material_count == 0)
    {
        BFATAL("material_system_initialize - config.max_material_count must be > 0");
        return false;
    }

    // Block of memory will contain state structure, then block for array, then block for hashtable
    u64 struct_requirement = sizeof(material_system_state);
    u64 array_requirement = sizeof(material) * typed_config->max_material_count;
    u64 hashtable_requirement = sizeof(material_reference) * typed_config->max_material_count;
    *memory_requirement = struct_requirement + array_requirement + hashtable_requirement;

    if (!state)
        return true;

    state_ptr = state;

    // Keep a pointer to the renderer system state for quick access
    state_ptr->renderer = engine_systems_get()->renderer_system;

    state_ptr->config = *typed_config;

    state_ptr->pbr_shader_id = INVALID_ID;

    // Array block is after the state. Already allocated, so just set the pointer
    void* array_block = state + struct_requirement;
    state_ptr->registered_materials = array_block;

    // Hashtable block is after array
    void* hashtable_block = array_block + array_requirement;

    // Create hashtable for material lookups
    hashtable_create(sizeof(material_reference), typed_config->max_material_count, hashtable_block, false, &state_ptr->registered_material_table);

    // Fill the hashtable with invalid references to use as a default
    material_reference invalid_ref;
    invalid_ref.auto_release = false;
    invalid_ref.handle = INVALID_ID;  // Primary reason for needing default values
    invalid_ref.reference_count = 0;
    hashtable_fill(&state_ptr->registered_material_table, &invalid_ref);

    // Invalidate all materials in the array
    u32 count = state_ptr->config.max_material_count;
    for (u32 i = 0; i < count; ++i)
    {
        state_ptr->registered_materials[i].id = INVALID_ID;
        state_ptr->registered_materials[i].generation = INVALID_ID;
        state_ptr->registered_materials[i].internal_id = INVALID_ID;
    }

    // Get uniform indices
    // Save locations for known types for quick lookups
    state_ptr->pbr_shader = shader_system_get("Shader.PBRMaterial");
    state_ptr->pbr_shader_id = state_ptr->pbr_shader->id;

    state_ptr->terrain_shader = shader_system_get("Shader.Builtin.Terrain");
    state_ptr->terrain_shader_id = state_ptr->terrain_shader->id;

    // Grab default cubemap texture as irradiance texture
    state_ptr->irradiance_cube_texture = texture_system_get_default_cube_texture();

    // Assign defaults
    for (u32 i = 0; i < MAX_SHADOW_CASCADE_COUNT; ++i)
        state_ptr->directional_light_space[i] = mat4_identity();

    // Load up some default materials
    if (!create_default_pbr_material(state_ptr))
    {
        BFATAL("Failed to create default PBR material. Application cannot continue");
        return false;
    }

    if (!create_default_terrain_material(state_ptr))
    {
        BFATAL("Failed to create default terrain material. Application cannot continue");
        return false;
    }

    // Register a console command to dump list of materials/references
    console_command_register("material_system_dump", 0, on_material_system_dump);

    return true;
}

void material_system_shutdown(void* state)
{
    material_system_state* s = (material_system_state*)state;
    if (s)
    {
        // Invalidate all materials in the array
        u32 count = s->config.max_material_count;
        for (u32 i = 0; i < count; ++i)
        {
            if (s->registered_materials[i].id != INVALID_ID)
                destroy_material(&s->registered_materials[i]);
        }

        // Destroy default material
        destroy_material(&s->default_pbr_material);
        destroy_material(&s->default_terrain_material);
    }

    state_ptr = 0;
}

material* material_system_acquire(const char* name)
{
    // Load material configuration from resource
    resource material_resource;
    if (!resource_system_load(name, RESOURCE_TYPE_MATERIAL, 0, &material_resource))
    {
        BERROR("Failed to load material resource, returning nullptr");
        return 0;
    }

    // Acquire from loaded config
    material* m = 0;
    if (material_resource.data)
        m = material_system_acquire_from_config((material_config*)material_resource.data);

    // Clean up
    resource_system_unload(&material_resource);

    if (!m)
        BERROR("Failed to load material resource, returning nullptr");

    return m;
}

static material* material_system_acquire_reference(const char* name, b8 auto_release, b8* needs_creation)
{
    material_reference ref;
    if (state_ptr && hashtable_get(&state_ptr->registered_material_table, name, &ref))
    {
        // This can only be changed the first time material is loaded
        if (ref.reference_count == 0)
            ref.auto_release = auto_release;
        ref.reference_count++;
        if (ref.handle == INVALID_ID)
        {
            // This means no material exists here. Find a free index first
            u32 count = state_ptr->config.max_material_count;
            material* m = 0;
            for (u32 i = 0; i < count; ++i)
            {
                if (state_ptr->registered_materials[i].id == INVALID_ID)
                {
                    // Free slot has been found. Use its index as handle
                    ref.handle = i;
                    m = &state_ptr->registered_materials[i];
                    break;
                }
            }

            // Make sure an empty slot was found
            if (!m || ref.handle == INVALID_ID)
            {
                BFATAL("material_system_acquire - Material system cannot hold anymore materials. Adjust configuration to allow more");
                return 0;
            }

            *needs_creation = true;

            // Also use handle as the material id
            m->id = ref.handle;
            // BTRACE("Material '%s' does not yet exist. Created, and ref_count is now %i", config->name, ref.reference_count);
        }
        else
        {
            // BTRACE("Material '%s' already exists, ref_count increased to %i", config->name, ref.reference_count);
            *needs_creation = false;
        }

        // Update entry
        hashtable_set(&state_ptr->registered_material_table, name, &ref);
        return &state_ptr->registered_materials[ref.handle];
    }

    // NOTE: This would only happen if something went wrong with the state
    BERROR("material_system_acquire_from_config failed to acquire material '%s'. Null pointer will be returned", name);
    return 0;
}

material* material_system_acquire_terrain_material(const char* material_name, u32 material_count, const char** material_names, b8 auto_release)
{
    // Return default terrain material
    if (strings_equali(material_name, DEFAULT_TERRAIN_MATERIAL_NAME))
        return &state_ptr->default_terrain_material;

    b8 needs_creation = false;
    material* m = material_system_acquire_reference(material_name, auto_release, &needs_creation);
    if (!m)
    {
        BERROR("Failed to acquire terrain material '%s'", material_name);
        return 0;
    }

    if (needs_creation)
    {
        // Gather material names
        const char** texture_names = ballocate(sizeof(const char*) * material_count * PBR_MATERIAL_TEXTURE_COUNT, MEMORY_TAG_ARRAY);
        for (u32 i = 0; i < material_count; ++i)
        {
            // Load material configuration from resource
            resource material_resource;
            if (!resource_system_load(material_names[i], RESOURCE_TYPE_MATERIAL, 0, &material_resource))
            {
                BERROR("Failed to load material resource, returning nullptr");
                return 0;
            }

            material_config* mat_config = (material_config*)material_resource.data;
            // NOTE: For now, PBR materials are required for terrains
            if (mat_config->type != MATERIAL_TYPE_PBR)
            {
                BERROR("Terrain materials must be PBR materials");
                return false;
            }

            // Extract the map names
            for (u32 j = 0; j < PBR_MATERIAL_TEXTURE_COUNT; ++j)
            {
                u32 index = (i * PBR_MATERIAL_TEXTURE_COUNT) + j;
                texture_names[index] = string_duplicate(mat_config->maps[j].texture_name);
            }

            // Clean up the resource
            resource_system_unload(&material_resource);
        }

        // Create new material
        // NOTE: terrain-specific load_material
        bzero_memory(m, sizeof(material));
        string_ncopy(m->name, material_name, MATERIAL_NAME_MAX_LENGTH);

        shader* selected_shader = state_ptr->terrain_shader;
        m->shader_id = selected_shader->id;
        m->type = MATERIAL_TYPE_TERRAIN;

        // Allocate maps and properties memory
        m->property_struct_size = sizeof(material_terrain_properties);
        m->properties = ballocate(m->property_struct_size, MEMORY_TAG_MATERIAL_INSTANCE);
        material_terrain_properties* properties = m->properties;
        properties->num_materials = material_count;
        properties->padding = vec3_zero();
        properties->padding2 = vec4_zero();

        // 3 maps per material for PBR. Allocate enough slots for all materials. Also 1 for irradiance map
        m->maps = darray_reserve(texture_map, TERRAIN_SAMP_COUNT);
        darray_length_set(m->maps, TERRAIN_SAMP_COUNT);

        // One map is needed for the entire material array
        {
            u32 layer_count = PBR_MATERIAL_TEXTURE_COUNT * MAX_TERRAIN_MATERIAL_COUNT;
            texture_map* map = &m->maps[SAMP_TERRAIN_MATERIAL_ARRAY_MAP];
            // TODO: Read this from config
            map->repeat_u = TEXTURE_REPEAT_REPEAT;
            map->repeat_v = TEXTURE_REPEAT_REPEAT;
            map->repeat_w = TEXTURE_REPEAT_REPEAT;
            map->filter_minify = TEXTURE_FILTER_MODE_LINEAR;
            map->filter_magnify = TEXTURE_FILTER_MODE_LINEAR;
            map->texture = texture_system_acquire_textures_as_arrayed(m->name, layer_count, texture_names, true);
            if (!map->texture)
            {
                // Configured, but not found
                BWARN("Unable to load arrayed texture '%s' for material '%s', using default", m->name, material_name);
                map->texture = texture_system_get_default_terrain_texture();
            }

            if (!renderer_texture_map_resources_acquire(map))
            {
                BERROR("Unable to acquire resources for texture map");
                return false;
            }
        }
        // Release texture names
        for (u32 i = 0; i < material_count; ++i)
        {
            for (u32 j = 0; j < PBR_MATERIAL_TEXTURE_COUNT; ++j)
            {
                u32 index = (i * PBR_MATERIAL_TEXTURE_COUNT) + j;
                string_free((char*)texture_names[index]);
            }
        }
        bfree(texture_names, sizeof(const char*) * material_count * PBR_MATERIAL_TEXTURE_COUNT, MEMORY_TAG_ARRAY);

        // Shadow maps can't be configured, so set them up here
        {
            material_map map_config = {0};
            map_config.filter_mag = map_config.filter_min = TEXTURE_FILTER_MODE_LINEAR;
            map_config.repeat_u = map_config.repeat_v = map_config.repeat_w = TEXTURE_REPEAT_CLAMP_TO_BORDER;
            map_config.name = "shadow_map";
            map_config.texture_name = "";
            if (!assign_map(&m->maps[SAMP_TERRAIN_SHADOW_MAP], &map_config, m->name, texture_system_get_default_diffuse_texture()))
            {
                BERROR("Failed to assign '%s' texture map for terrain shadow map", map_config.name);
                return false;
            }
        }

        // IBL - cubemap for irradiance
        {
            material_map map_config = {0};
            map_config.filter_mag = map_config.filter_min = TEXTURE_FILTER_MODE_LINEAR;
            map_config.repeat_u = map_config.repeat_v = map_config.repeat_w = TEXTURE_REPEAT_REPEAT;
            map_config.name = "ibl_cube";
            map_config.texture_name = "";
            // Always assigned to the last index
            if (!assign_map(&m->maps[SAMP_TERRAIN_IRRADIANCE_MAP], &map_config, m->name, texture_system_get_default_cube_texture()))
            {
                BERROR("Failed to assign '%s' texture map for terrain irradiance map", map_config.name);
                return false;
            }
        }

        // NOTE: 4 materials * 3 maps will still be loaded in order (albedo/norm/combined(met/rough/ao) per mat)
        // Next group will be shadow mappings
        // Last irradiance map

        // Setup a configuration to get instance resources for this material
        shader_instance_resource_config instance_resource_config = {0};
        // Map count for this type is known
        instance_resource_config.uniform_config_count = 3;  // NOTE: This includes material maps, shadow maps and irradiance map
        instance_resource_config.uniform_configs = ballocate(sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

        // Material textures (single array texture)
        shader_instance_uniform_texture_config* mat_textures = &instance_resource_config.uniform_configs[0];
        // mat_textures->uniform_location = state_ptr->terrain_locations.material_texures;
        mat_textures->texture_map_count = 1;
        mat_textures->texture_maps = ballocate(sizeof(texture_map*) * mat_textures->texture_map_count, MEMORY_TAG_ARRAY);
        mat_textures->texture_maps[0] = &m->maps[SAMP_TERRAIN_MATERIAL_ARRAY_MAP];

        // Shadow textures
        shader_instance_uniform_texture_config* shadow_textures = &instance_resource_config.uniform_configs[1];
        // shadow_textures->uniform_location = state_ptr->terrain_locations.shadow_textures;
        shadow_textures->texture_map_count = 1;
        shadow_textures->texture_maps = ballocate(sizeof(texture_map*) * shadow_textures->texture_map_count, MEMORY_TAG_ARRAY);
        shadow_textures->texture_maps[0] = &m->maps[SAMP_TERRAIN_SHADOW_MAP];

        // IBL cube texture
        shader_instance_uniform_texture_config* ibl_cube_texture = &instance_resource_config.uniform_configs[2];
        // ibl_cube_texture->uniform_location = state_ptr->terrain_locations.ibl_cube_texture;
        ibl_cube_texture->texture_map_count = 1;
        ibl_cube_texture->texture_maps = ballocate(sizeof(texture_map*) * ibl_cube_texture->texture_map_count, MEMORY_TAG_ARRAY);
        ibl_cube_texture->texture_maps[0] = &m->maps[SAMP_TERRAIN_IRRADIANCE_MAP];

        // Acquire the resources
        b8 result = renderer_shader_instance_resources_acquire(state_ptr->renderer, selected_shader, &instance_resource_config, &m->internal_id);
        if (!result)
            BERROR("Failed to acquire renderer resources for terrain material '%s'", m->name);

        // Clean up the uniform configs
        for (u32 i = 0; i < instance_resource_config.uniform_config_count; ++i)
        {
            shader_instance_uniform_texture_config* ucfg = &instance_resource_config.uniform_configs[i];
            bfree(ucfg->texture_maps, sizeof(shader_instance_uniform_texture_config) * ucfg->texture_map_count, MEMORY_TAG_ARRAY);
            ucfg->texture_maps = 0;
        }
        bfree(instance_resource_config.uniform_configs, sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);
        // NOTE: end terrain-specific load_material

        if (m->generation == INVALID_ID)
            m->generation = 0;
        else
            m->generation++;
    }

    return m;
}

material* material_system_acquire_from_config(material_config* config)
{
    // Return default material
    if (strings_equali(config->name, DEFAULT_PBR_MATERIAL_NAME))
        return &state_ptr->default_pbr_material;

    // Return default terrain material
    if (strings_equali(config->name, DEFAULT_TERRAIN_MATERIAL_NAME))
        return &state_ptr->default_terrain_material;

    b8 needs_creation = false;
    material* m = material_system_acquire_reference(config->name, config->auto_release, &needs_creation);

    if (needs_creation)
    {
        // Create new material
        if (!load_material(config, m))
        {
            BERROR("Failed to load material '%s'", config->name);
            return 0;
        }

        if (m->generation == INVALID_ID)
            m->generation = 0;
        else
            m->generation++;
    }

    return m;
}

void material_system_release(const char* name)
{
    // Ignore release requests for the default material
    if (strings_equali(name, DEFAULT_PBR_MATERIAL_NAME) || strings_equali(name, DEFAULT_TERRAIN_MATERIAL_NAME))
        return;
    material_reference ref;
    if (state_ptr && hashtable_get(&state_ptr->registered_material_table, name, &ref))
    {
        if (ref.reference_count == 0)
        {
            BWARN("Tried to release non-existent material: '%s'", name);
            return;
        }

        // Take a copy of the name since it would be wiped out if destroyed
        char name_copy[MATERIAL_NAME_MAX_LENGTH];
        string_ncopy(name_copy, name, MATERIAL_NAME_MAX_LENGTH);

        ref.reference_count--;
        if (ref.reference_count == 0 && ref.auto_release)
        {
            material* m = &state_ptr->registered_materials[ref.handle];

            // Destroy/reset material
            destroy_material(m);

            // This makes the reference slot "available"
            ref.handle = INVALID_ID;

            // Reset reference
            // BTRACE("Released material '%s', Material unloaded because reference count=0 and auto_release=true", name_copy);
        }
        else
        {
            // BTRACE("Released material '%s', now has a reference count of '%i' (auto_release=%s)", name_copy, ref.reference_count, ref.auto_release ? "true" : "false");
        }

        // Update entry
        hashtable_set(&state_ptr->registered_material_table, name_copy, &ref);
    }
    else
    {
        BERROR("material_system_release failed to release material '%s'", name);
    }
}

material* material_system_get_default(void)
{
    return material_system_get_default_pbr();
}

material* material_system_get_default_pbr(void)
{
    if (state_ptr)
        return &state_ptr->default_pbr_material;

    BFATAL("material_system_get_default_pbr called before system is initialized");
    return 0;
}

material* material_system_get_default_terrain(void)
{
    if (state_ptr)
        return &state_ptr->default_terrain_material;

    BFATAL("material_system_get_default_terrain called before system is initialized");
    return 0;
}

void material_system_dump(void)
{
    material_reference* refs = (material_reference*)state_ptr->registered_material_table.memory;
    for (u32 i = 0; i < state_ptr->registered_material_table.element_count; ++i)
    {
        material_reference* r = &refs[i];
        if (r->reference_count > 0 || r->handle != INVALID_ID)
        {
            BDEBUG("Found material ref (handle/refCount): (%u/%u)", r->handle, r->reference_count);
            if (r->handle != INVALID_ID)
                BTRACE("Material name: %s", state_ptr->registered_materials[r->handle].name);
        }
    }
}

static b8 assign_map(texture_map* map, const material_map* config, const char* material_name, texture* default_tex)
{
    map->filter_minify = config->filter_min;
    map->filter_magnify = config->filter_mag;
    map->repeat_u = config->repeat_u;
    map->repeat_v = config->repeat_v;
    map->repeat_w = config->repeat_w;

    if (string_length(config->texture_name) > 0)
    {
        map->texture = texture_system_acquire(config->texture_name, true);
        if (!map->texture)
        {
            // Configured, but not found
            BWARN("Unable to load texture '%s' for material '%s', using default", config->texture_name, material_name);
            map->texture = default_tex;
        }
    }
    else
    {
        // This is done when a texture is not configured, as opposed to when it is configured and not found (above)
        map->texture = default_tex;
    }
    if (!renderer_texture_map_resources_acquire(map))
    {
        BERROR("Unable to acquire resources for texture map");
        return false;
    }
    return true;
}

static b8 load_material(material_config* config, material* m)
{
    bzero_memory(m, sizeof(material));

    // Name
    string_ncopy(m->name, config->name, MATERIAL_NAME_MAX_LENGTH);

    m->type = config->type;
    shader* selected_shader = 0;
    shader_instance_resource_config instance_resource_config = {0};

    // Process the material config by type
    if (config->type == MATERIAL_TYPE_PBR)
    {
        selected_shader = state_ptr->pbr_shader;
        m->shader_id = state_ptr->pbr_shader_id;
        // PBR-specific properties
        u32 prop_count = darray_length(config->properties);

        // Defaults
        // TODO: PBR properties
        m->property_struct_size = sizeof(material_phong_properties);
        m->properties = ballocate(sizeof(material_phong_properties), MEMORY_TAG_MATERIAL_INSTANCE);
        material_phong_properties* properties = (material_phong_properties*)m->properties;
        properties->diffuse_color = vec4_one();
        properties->shininess = 32.0f;
        properties->padding = vec3_zero();
        for (u32 i = 0; i < prop_count; ++i)
        {
            if (strings_equali(config->properties[i].name, "diffuse_color"))
                properties->diffuse_color = config->properties[i].value_v4;
            else if (strings_equali(config->properties[i].name, "shininess"))
                properties->shininess = config->properties[i].value_f32;
        }

        // Maps. PBR expects albedo, normal, combined (metallic, roughness, AO)
        m->maps = darray_reserve(texture_map, PBR_MAP_COUNT);
        darray_length_set(m->maps, PBR_MAP_COUNT);
        u32 configure_map_count = darray_length(config->maps);

        // Setup tracking for what maps are/are not assigned
        b8 mat_maps_assigned[PBR_MATERIAL_TEXTURE_COUNT];
        for (u32 i = 0; i < PBR_MATERIAL_TEXTURE_COUNT; ++i)
            mat_maps_assigned[i] = false;
        b8 ibl_cube_assigned = false;
        const char* map_names[PBR_MATERIAL_TEXTURE_COUNT] = {"albedo", "normal", "combined"};
        texture* default_textures[PBR_MATERIAL_TEXTURE_COUNT] = {
            texture_system_get_default_diffuse_texture(),
            texture_system_get_default_normal_texture(),
            texture_system_get_default_combined_texture()
        };

        // Attempt to match configured names to those required by PBR materials
        // This also ensures the maps are in the proper order
        for (u32 i = 0; i < configure_map_count; ++i)
        {
            b8 found = false;
            for (u32 tex_slot = 0; tex_slot < PBR_MATERIAL_TEXTURE_COUNT; ++tex_slot) {
                if (strings_equali(config->maps[i].name, map_names[tex_slot])) {
                    if (!assign_map(&m->maps[tex_slot], &config->maps[i], m->name, default_textures[tex_slot])) {
                        return false;
                    }
                    mat_maps_assigned[tex_slot] = true;
                    found = true;
                    break;
                }
            }
            if (found)
                continue;
            // See if it is a configured IBL cubemap
            // TODO: May not want this to be configurable as a map, but rather provided by the scene from a reflection probe
            if (strings_equali(config->maps[i].name, "ibl_cube"))
            {
                if (!assign_map(&m->maps[SAMP_IRRADIANCE_MAP], &config->maps[i], m->name, texture_system_get_default_cube_texture()))
                    return false;
                ibl_cube_assigned = true;
            }
            else
            {
                // NOTE: Ignore unexpected maps, but warn about it
                BWARN("Configuration for material '%s' contains a map named '%s', which will be ignored for PBR material types", config->name, config->maps[i].name);
            }
        }
        // Ensure all maps are always assigned, even if only with defaults
        for (u32 i = 0; i < PBR_MATERIAL_TEXTURE_COUNT; ++i)
        {
            if (!mat_maps_assigned[i])
            {
                material_map map_config = {0};
                map_config.filter_mag = map_config.filter_min = TEXTURE_FILTER_MODE_LINEAR;
                map_config.repeat_u = map_config.repeat_v = map_config.repeat_w = TEXTURE_REPEAT_REPEAT;
                map_config.name = string_duplicate(map_names[i]);
                map_config.texture_name = "";
                b8 assign_result = assign_map(&m->maps[i], &map_config, m->name, default_textures[i]);
                string_free(map_config.name);
                if (!assign_result)
                    return false;
            }
        }

        // Also make sure the cube map is always assigned
        // TODO: May not want this to be configurable as a map, but rather provided by the scene from a reflection probe
        if (!ibl_cube_assigned)
        {
            material_map map_config = {0};
            map_config.filter_mag = map_config.filter_min = TEXTURE_FILTER_MODE_LINEAR;
            map_config.repeat_u = map_config.repeat_v = map_config.repeat_w = TEXTURE_REPEAT_REPEAT;
            map_config.name = "ibl_cube";
            map_config.texture_name = "";
            if (!assign_map(&m->maps[SAMP_IRRADIANCE_MAP], &map_config, m->name, texture_system_get_default_cube_texture()))
                return false;
        }

        // Shadow maps can't be configured, so set them up here
        {
            material_map map_config = {0};
            map_config.filter_mag = map_config.filter_min = TEXTURE_FILTER_MODE_LINEAR;
            map_config.repeat_u = map_config.repeat_v = map_config.repeat_w = TEXTURE_REPEAT_CLAMP_TO_BORDER;
            map_config.name = "shadow_map";
            map_config.texture_name = "";
            if (!assign_map(&m->maps[SAMP_SHADOW_MAP], &map_config, m->name, texture_system_get_default_diffuse_texture()))
                return false;
        }

        // Gather a list of pointers to texture maps
        // Send it off to the renderer to acquire resources
        // Map count for this type is known
        instance_resource_config.uniform_config_count = 3;  // NOTE: This includes material maps, shadow maps and irradiance map
        instance_resource_config.uniform_configs = ballocate(sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

        // Material textures
        shader_instance_uniform_texture_config* mat_textures = &instance_resource_config.uniform_configs[0];
        // mat_textures->uniform_location = state_ptr->pbr_locations.material_texures;
        mat_textures->texture_map_count = PBR_MATERIAL_TEXTURE_COUNT;
        mat_textures->texture_maps = ballocate(sizeof(texture_map*) * mat_textures->texture_map_count, MEMORY_TAG_ARRAY);
        mat_textures->texture_maps[SAMP_ALBEDO] = &m->maps[SAMP_ALBEDO];
        mat_textures->texture_maps[SAMP_NORMAL] = &m->maps[SAMP_NORMAL];
        mat_textures->texture_maps[SAMP_COMBINED] = &m->maps[SAMP_COMBINED];

        // Shadow textures
        shader_instance_uniform_texture_config* shadow_textures = &instance_resource_config.uniform_configs[1];
        // shadow_textures->uniform_location = state_ptr->pbr_locations.shadow_textures;
        shadow_textures->texture_map_count = 1;
        shadow_textures->texture_maps = ballocate(sizeof(texture_map*) * shadow_textures->texture_map_count, MEMORY_TAG_ARRAY);
        shadow_textures->texture_maps[0] = &m->maps[SAMP_SHADOW_MAP];

        // IBL cube texture
        shader_instance_uniform_texture_config* ibl_cube_texture = &instance_resource_config.uniform_configs[2];
        // ibl_cube_texture->uniform_location = state_ptr->pbr_locations.ibl_cube_texture;
        ibl_cube_texture->texture_map_count = 1;
        ibl_cube_texture->texture_maps = ballocate(sizeof(texture_map*) * ibl_cube_texture->texture_map_count, MEMORY_TAG_ARRAY);
        ibl_cube_texture->texture_maps[0] = &m->maps[SAMP_IRRADIANCE_MAP];
    }
    else if (config->type == MATERIAL_TYPE_CUSTOM)
    {
        // Gather a list of pointers to texture maps
        // Send it off to the renderer to acquire resources
        // Custom materials
        if (!config->shader_name)
        {
            BERROR("Shader name is required for custom material types. Material '%s' failed to load", m->name);
            return false;
        }
        selected_shader = shader_system_get(config->shader_name);
        m->shader_id = selected_shader->id;
        // Properties
        u32 prop_count = darray_length(config->properties);
        // Start by getting a total size of all properties
        m->property_struct_size = 0;
        for (u32 i = 0; i < prop_count; ++i)
        {
            if (config->properties[i].size > 0)
                m->property_struct_size += config->properties[i].size;
        }

        // Allocate enough space for the struct
        m->properties = ballocate(m->property_struct_size, MEMORY_TAG_MATERIAL_INSTANCE);

        // Loop again and copy values to the struct
        // NOTE: There are no defaults for custom material uniforms
        u32 offset = 0;
        for (u32 i = 0; i < prop_count; ++i)
        {
            if (config->properties[i].size > 0)
            {
                void* data = 0;
                switch (config->properties[i].type)
                {
                    case SHADER_UNIFORM_TYPE_INT8:
                        data = &config->properties[i].value_i8;
                        break;
                    case SHADER_UNIFORM_TYPE_UINT8:
                        data = &config->properties[i].value_u8;
                        break;
                    case SHADER_UNIFORM_TYPE_INT16:
                        data = &config->properties[i].value_i16;
                        break;
                    case SHADER_UNIFORM_TYPE_UINT16:
                        data = &config->properties[i].value_u16;
                        break;
                    case SHADER_UNIFORM_TYPE_INT32:
                        data = &config->properties[i].value_i32;
                        break;
                    case SHADER_UNIFORM_TYPE_UINT32:
                        data = &config->properties[i].value_u32;
                        break;
                    case SHADER_UNIFORM_TYPE_FLOAT32:
                        data = &config->properties[i].value_f32;
                        break;
                    case SHADER_UNIFORM_TYPE_FLOAT32_2:
                        data = &config->properties[i].value_v2;
                        break;
                    case SHADER_UNIFORM_TYPE_FLOAT32_3:
                        data = &config->properties[i].value_v3;
                        break;
                    case SHADER_UNIFORM_TYPE_FLOAT32_4:
                        data = &config->properties[i].value_v4;
                        break;
                    case SHADER_UNIFORM_TYPE_MATRIX_4:
                        data = &config->properties[i].value_mat4;
                        break;
                    default:
                        BWARN("Unable to process shader uniform type %d (index %u) for material '%s'. Skipping", config->properties[i].type, i, m->name);
                        continue;
                }

                // Copy block and move up
                bcopy_memory(m->properties + offset, data, config->properties[i].size);
                offset += config->properties[i].size;
            }
        }

        // Maps. Custom materials can have any number of maps
        u32 map_count = darray_length(config->maps);
        m->maps = darray_reserve(texture_map, map_count);
        darray_length_set(m->maps, map_count);
        for (u32 i = 0; i < map_count; ++i)
        {
            // No known mapping, so just map them in order
            // Invalid textures will use default texture, because map type isn't known
            if (!assign_map(&m->maps[i], &config->maps[i], m->name, texture_system_get_default_texture()))
                return false;
        }

        u32 global_sampler_count = selected_shader->global_uniform_sampler_count;
        u32 instance_sampler_count = selected_shader->instance_uniform_sampler_count;

        // NOTE: The map order for custom materials must match the uniform sampler order defined in the shader
        instance_resource_config.uniform_config_count = global_sampler_count + instance_sampler_count;
        instance_resource_config.uniform_configs = ballocate(sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

        // Track the number of maps used by global uniforms first and offset by that
        u32 map_offset = 0;
        for (u32 i = 0; i < global_sampler_count; ++i)
            map_offset++;

        for (u32 i = 0; i < instance_sampler_count; ++i)
        {
            shader_uniform* u = &selected_shader->uniforms[selected_shader->instance_sampler_indices[i]];
            shader_instance_uniform_texture_config* uniform_config = &instance_resource_config.uniform_configs[i];
            // uniform_config->uniform_location = u->location;
            uniform_config->texture_map_count = BMAX(u->array_length, 1);
            uniform_config->texture_maps = ballocate(sizeof(texture_map*) * uniform_config->texture_map_count, MEMORY_TAG_ARRAY);
            for (u32 j = 0; j < uniform_config->texture_map_count; ++j)
            {
                uniform_config->texture_maps[j] = &m->maps[i + map_offset];
            }
        }
    }
    else
    {
        BERROR("Unknown material type: %d. Material '%s' cannot be loaded", config->type, m->name);
        return false;
    }

    // Acquire instance resources for this material
    b8 result = renderer_shader_instance_resources_acquire(state_ptr->renderer, selected_shader, &instance_resource_config, &m->internal_id);
    if (!result)
        BERROR("Failed to acquire renderer resources for material '%s'", m->name);

    // Clean up the uniform configs
    for (u32 i = 0; i < instance_resource_config.uniform_config_count; ++i)
    {
        shader_instance_uniform_texture_config* ucfg = &instance_resource_config.uniform_configs[i];
        bfree(ucfg->texture_maps, sizeof(shader_instance_uniform_texture_config) * ucfg->texture_map_count, MEMORY_TAG_ARRAY);
        ucfg->texture_maps = 0;
    }
    bfree(instance_resource_config.uniform_configs, sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

    return result;
}

static void destroy_material(material* m)
{
    // BTRACE("Destroying material '%s'...", m->name);

    u32 length = darray_length(m->maps);
    for (u32 i = 0; i < length; ++i)
    {
        // Release texture references
        if (m->maps[i].texture)
            texture_system_release(m->maps[i].texture->name);
        // Release texture map resources
        renderer_texture_map_resources_release(&m->maps[i]);
    }

    // Release renderer resources
    if (m->shader_id != INVALID_ID && m->internal_id != INVALID_ID)
    {
        renderer_shader_instance_resources_release(state_ptr->renderer, shader_system_get_by_id(m->shader_id), m->internal_id);
        m->shader_id = INVALID_ID;
    }

    // Release properties
    if (m->properties && m->property_struct_size)
        bfree(m->properties, m->property_struct_size, MEMORY_TAG_MATERIAL_INSTANCE);

    // Zero it out, invalidate IDs
    bzero_memory(m, sizeof(material));
    m->id = INVALID_ID;
    m->generation = INVALID_ID;
    m->internal_id = INVALID_ID;
}

static b8 create_default_pbr_material(material_system_state* state)
{
    bzero_memory(&state->default_pbr_material, sizeof(material));
    state->default_pbr_material.id = INVALID_ID;
    state->default_pbr_material.type = MATERIAL_TYPE_PBR;
    state->default_pbr_material.generation = INVALID_ID;
    string_ncopy(state->default_pbr_material.name, DEFAULT_PBR_MATERIAL_NAME, MATERIAL_NAME_MAX_LENGTH);
    // TODO: material PBR properties
    state->default_pbr_material.property_struct_size = sizeof(material_phong_properties);
    state->default_pbr_material.properties = ballocate(sizeof(material_phong_properties), MEMORY_TAG_MATERIAL_INSTANCE);
    material_phong_properties* properties = (material_phong_properties*)state->default_pbr_material.properties;
    properties->diffuse_color = vec4_one();  // white
    properties->shininess = 8.0f;
    state->default_pbr_material.maps = darray_reserve(texture_map, PBR_MAP_COUNT);
    darray_length_set(state->default_pbr_material.maps, PBR_MAP_COUNT);
    for (u32 i = 0; i < PBR_MAP_COUNT; ++i)
    {
        texture_map* map = &state->default_pbr_material.maps[i];
        if (i == 0)
        {
            // NOTE: setting mode to nearest neighbor to make the chekerboard non-blurry
            map->filter_magnify = map->filter_minify = TEXTURE_FILTER_MODE_NEAREST;
        }
        map->filter_magnify = map->filter_minify = TEXTURE_FILTER_MODE_LINEAR;
        map->repeat_u = map->repeat_v = map->repeat_w = TEXTURE_REPEAT_REPEAT;
    }

    // Change clamp mode on the default shadow map to border
    texture_map* ssm = &state->default_pbr_material.maps[SAMP_SHADOW_MAP];
    ssm->repeat_u = ssm->repeat_v = ssm->repeat_w = TEXTURE_REPEAT_CLAMP_TO_BORDER;

    state->default_pbr_material.maps[SAMP_ALBEDO].texture = texture_system_get_default_texture();
    state->default_pbr_material.maps[SAMP_NORMAL].texture = texture_system_get_default_normal_texture();
    state->default_pbr_material.maps[SAMP_COMBINED].texture = texture_system_get_default_combined_texture();
    state->default_pbr_material.maps[SAMP_SHADOW_MAP].texture = texture_system_get_default_diffuse_texture();
    state->default_pbr_material.maps[SAMP_IRRADIANCE_MAP].texture = texture_system_get_default_cube_texture();

    // Setup a configuration to get instance resources for this material
    material* m = &state->default_pbr_material;
    shader_instance_resource_config instance_resource_config = {0};
    // Map count for this type is known
    instance_resource_config.uniform_config_count = 3;  // NOTE: This includes material maps, shadow maps and irradiance map
    instance_resource_config.uniform_configs = ballocate(sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

    // Material textures
    shader_instance_uniform_texture_config* mat_textures = &instance_resource_config.uniform_configs[0];
    // mat_textures->uniform_location = state_ptr->pbr_locations.material_texures;
    mat_textures->texture_map_count = PBR_MATERIAL_TEXTURE_COUNT;
    mat_textures->texture_maps = ballocate(sizeof(texture_map*) * mat_textures->texture_map_count, MEMORY_TAG_ARRAY);
    mat_textures->texture_maps[SAMP_ALBEDO] = &m->maps[SAMP_ALBEDO];
    mat_textures->texture_maps[SAMP_NORMAL] = &m->maps[SAMP_NORMAL];
    mat_textures->texture_maps[SAMP_COMBINED] = &m->maps[SAMP_COMBINED];

    // Shadow textures
    shader_instance_uniform_texture_config* shadow_textures = &instance_resource_config.uniform_configs[1];
    // shadow_textures->uniform_location = state_ptr->pbr_locations.shadow_textures;
    shadow_textures->texture_map_count = 1;
    shadow_textures->texture_maps = ballocate(sizeof(texture_map*) * shadow_textures->texture_map_count, MEMORY_TAG_ARRAY);
    shadow_textures->texture_maps[0] = &m->maps[SAMP_SHADOW_MAP];

    // IBL cube texture
    shader_instance_uniform_texture_config* ibl_cube_texture = &instance_resource_config.uniform_configs[2];
    // ibl_cube_texture->uniform_location = state_ptr->pbr_locations.ibl_cube_texture;
    ibl_cube_texture->texture_map_count = 1;
    ibl_cube_texture->texture_maps = ballocate(sizeof(texture_map*) * ibl_cube_texture->texture_map_count, MEMORY_TAG_ARRAY);
    ibl_cube_texture->texture_maps[0] = &m->maps[SAMP_IRRADIANCE_MAP];

    shader* s = shader_system_get_by_id(state_ptr->pbr_shader_id);
    if (!renderer_shader_instance_resources_acquire(state_ptr->renderer, s, &instance_resource_config, &state->default_pbr_material.internal_id))
    {
        BFATAL("Failed to acquire renderer resources for default PBR material. Application cannot continue");
        return false;
    }

    // Clean up the uniform configs
    for (u32 i = 0; i < instance_resource_config.uniform_config_count; ++i)
    {
        shader_instance_uniform_texture_config* ucfg = &instance_resource_config.uniform_configs[i];
        bfree(ucfg->texture_maps, sizeof(ucfg->texture_maps[0]) * ucfg->texture_map_count, MEMORY_TAG_ARRAY);
        ucfg->texture_maps = 0;
    }
    bfree(instance_resource_config.uniform_configs, sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

    // Make sure to assign the shader id
    state->default_pbr_material.shader_id = s->id;

    return true;
}

static b8 create_default_terrain_material(material_system_state* state)
{
    bzero_memory(&state->default_terrain_material, sizeof(material));
    state->default_terrain_material.id = INVALID_ID;
    state->default_terrain_material.type = MATERIAL_TYPE_TERRAIN;
    state->default_terrain_material.generation = INVALID_ID;
    string_ncopy(state->default_terrain_material.name, DEFAULT_TERRAIN_MATERIAL_NAME, MATERIAL_NAME_MAX_LENGTH);

    // Should be same thing as defualt material, just mapped to an "array" of one material
    state->default_terrain_material.property_struct_size = sizeof(material_terrain_properties);
    state->default_terrain_material.properties = ballocate(sizeof(material_terrain_properties), MEMORY_TAG_MATERIAL_INSTANCE);
    material_terrain_properties* properties = (material_terrain_properties*)state->default_terrain_material.properties;
    properties->num_materials = MAX_TERRAIN_MATERIAL_COUNT;
    properties->materials[0].diffuse_color = vec4_one();  // white
    properties->materials[0].shininess = 8.0f;
    state->default_terrain_material.maps = darray_reserve(texture_map, TERRAIN_SAMP_COUNT);
    darray_length_set(state->default_terrain_material.maps, TERRAIN_SAMP_COUNT);
    // Material texture array
    texture_map* map = &state->default_terrain_material.maps[SAMP_TERRAIN_MATERIAL_ARRAY_MAP];
    map->texture = texture_system_get_default_terrain_texture();
    // NOTE: setting mode to nearest neighbor to make the chekerboard non-blurry
    map->filter_magnify = map->filter_minify = TEXTURE_FILTER_MODE_NEAREST;
    state->default_terrain_material.maps[SAMP_TERRAIN_SHADOW_MAP].texture = texture_system_get_default_diffuse_texture();

    // Change clamp mode on the default shadow map to border
    texture_map* ssm = &state->default_terrain_material.maps[SAMP_TERRAIN_SHADOW_MAP];
    ssm->repeat_u = ssm->repeat_v = ssm->repeat_w = TEXTURE_REPEAT_CLAMP_TO_BORDER;

    // NOTE: PBR materials are required for terrains
    // NOTE: 4 materials * 3 maps will still be loaded in order (albedo/norm/met/rough/ao per mat)
    // Next group will be shadow mappings
    // Last irradiance map

    // Setup a configuration to get instance resources for this material
    shader_instance_resource_config instance_resource_config = {0};
    // Map count for this type is known
    instance_resource_config.uniform_config_count = 3;  // NOTE: This includes material maps, shadow maps and irradiance map
    instance_resource_config.uniform_configs = ballocate(sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

    // Material textures
    material* m = &state_ptr->default_terrain_material;
    shader_instance_uniform_texture_config* mat_textures = &instance_resource_config.uniform_configs[0];
    // mat_textures->uniform_location = state_ptr->terrain_locations.material_texures;
    mat_textures->texture_map_count = 1;
    mat_textures->texture_maps = ballocate(sizeof(texture_map*) * mat_textures->texture_map_count, MEMORY_TAG_ARRAY);
    mat_textures->texture_maps[SAMP_TERRAIN_MATERIAL_ARRAY_MAP] = &m->maps[SAMP_TERRAIN_MATERIAL_ARRAY_MAP];

    // Shadow textures
    shader_instance_uniform_texture_config* shadow_textures = &instance_resource_config.uniform_configs[1];
    // shadow_textures->uniform_location = state_ptr->terrain_locations.shadow_textures;
    shadow_textures->texture_map_count = 1;
    shadow_textures->texture_maps = ballocate(sizeof(texture_map*) * shadow_textures->texture_map_count, MEMORY_TAG_ARRAY);
    shadow_textures->texture_maps[0] = &m->maps[SAMP_TERRAIN_SHADOW_MAP];

    // IBL cube texture
    shader_instance_uniform_texture_config* ibl_cube_texture = &instance_resource_config.uniform_configs[2];
    // ibl_cube_texture->uniform_location = state_ptr->terrain_locations.ibl_cube_texture;
    ibl_cube_texture->texture_map_count = 1;
    ibl_cube_texture->texture_maps = ballocate(sizeof(texture_map*) * ibl_cube_texture->texture_map_count, MEMORY_TAG_ARRAY);
    ibl_cube_texture->texture_maps[0] = &m->maps[SAMP_TERRAIN_IRRADIANCE_MAP];

    // Acquire the resources
    shader* s = shader_system_get_by_id(state_ptr->terrain_shader_id);
    b8 result = renderer_shader_instance_resources_acquire(state_ptr->renderer, s, &instance_resource_config, &state->default_terrain_material.internal_id);
    if (!result)
        BERROR("Failed to acquire renderer resources for default terrain material '%s'");

    // Clean up the uniform configs
    for (u32 i = 0; i < instance_resource_config.uniform_config_count; ++i)
    {
        shader_instance_uniform_texture_config* ucfg = &instance_resource_config.uniform_configs[i];
        bfree(ucfg->texture_maps, sizeof(ucfg->texture_maps[0]) * ucfg->texture_map_count, MEMORY_TAG_ARRAY);
        ucfg->texture_maps = 0;
    }
    bfree(instance_resource_config.uniform_configs, sizeof(shader_instance_uniform_texture_config) * instance_resource_config.uniform_config_count, MEMORY_TAG_ARRAY);

    // Make sure to assign shader id
    state->default_terrain_material.shader_id = s->id;

    return true;
}

static void on_material_system_dump(console_command_context context)
{
    material_system_dump();
}
