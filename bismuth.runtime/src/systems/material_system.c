#include "material_system.h"

#include "core/console.h"
#include "core/engine.h"
#include "debug/bassert.h"
#include "defines.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "renderer/renderer_frontend.h"
#include "resources/resource_types.h"
#include "strings/bname.h"
#include "strings/bstring.h"
#include "systems/bresource_system.h"
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

// The number of texture maps for a PBR material
#define PBR_MATERIAL_MAP_COUNT 3

// Number of texture maps for a layered PBR material
#define LAYERED_PBR_MATERIAL_MAP_COUNT 1

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

    bresource_material* default_unlit_material;
    bresource_material* default_phong_material;
    bresource_material* default_pbr_material;
    bresource_material* default_layered_material;

    // FIXME: remove this
    u32 terrain_shader_id;
    shader* terrain_shader;
    u32 pbr_shader_id;
    shader* pbr_shader;

    // Keep a pointer to the renderer state for quick access
    struct renderer_system_state* renderer;
    struct texture_system_state* texture_system;
    struct bresource_system_state* resource_system;
} material_system_state;

static b8 create_default_pbr_material(material_system_state* state);
static b8 create_default_terrain_material(material_system_state* state);
static b8 load_material(material_system_state* state, material_config* config, material* m);
static void destroy_material(bresource_material* m);

static b8 assign_map(material_system_state* state, bresource_texture_map* map, const material_map* config, bname material_name, const bresource_texture* default_tex);
static void on_material_system_dump(console_command_context context);

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

    // FIXME: remove these
    // Get uniform indices
    // Save locations for known types for quick lookups
    state->pbr_shader = shader_system_get("Shader.PBRMaterial");
    state->pbr_shader_id = state->pbr_shader->id;
    state->terrain_shader = shader_system_get("Shader.Builtin.Terrain");
    state->terrain_shader_id = state->terrain_shader->id;

    // Load up some default materials
    if (!create_default_pbr_material(state))
    {
        BFATAL("Failed to create default PBR material. Application cannot continue");
        return false;
    }

    if (!create_default_terrain_material(state))
    {
        BFATAL("Failed to create default terrain material. Application cannot continue");
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
        // Release default materials
        bresource_system_release(state->resource_system, state->default_pbr_material->base.name);
        bresource_system_release(state->resource_system, state->default_layered_material->base.name);
    }
}

static void material_resource_loaded(bresource* resource, void* listener)
{
    bresource_material* typed_resource = (bresource_material*)resource;
    material_instance* instance = (material_instance*)listener;

    if (resource->state == BRESOURCE_STATE_LOADED)
    {
        if (typed_resource->type == BRESOURCE_MATERIAL_TYPE_PBR)
        {
            u32 pbr_shader_id = shader_system_get_id("Shader.PBRMaterial");
            // NOTE: No maps for this shader type
            if (!shader_system_shader_per_draw_acquire(pbr_shader_id, 1, 0, &instance->per_draw_id))
            {
                BASSERT_MSG(false, "Failed to acquire renderer resources for default PBR material. Application cannot continue");
            }
        }
        else
        {
            BASSERT_MSG(false, "Unsupported material type - add local shader acquisition logic");
        }
    }
}

b8 material_system_acquire(material_system_state* state, bname name, material_instance* out_instance)
{
    BASSERT_MSG(out_instance, "out_instance is required");

    bresource_material_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_MATERIAL;
    request.base.user_callback = material_resource_loaded;
    request.base.listener_inst = out_instance;
    out_instance->material = (bresource_material*)bresource_system_request(state->resource_system, name, (bresource_request_info*)&request);
    return true;
}

void material_system_release_instance(material_system_state* state, material_instance* instance)
{
    if (instance)
    {
        u32 shader_id;
        b8 do_release = true;
        switch (instance->material->type)
        {
        default:
        case BRESOURCE_MATERIAL_TYPE_UNKNOWN:
            BWARN("Unknown material type - per-draw resources cannot be released");
            return;
        case BRESOURCE_MATERIAL_TYPE_UNLIT:
            shader_id = shader_system_get_id("Shader.Unlit");
            do_release = instance->material != state->default_unlit_material;
            break;
        case BRESOURCE_MATERIAL_TYPE_PHONG:
            shader_id = shader_system_get_id("Shader.Phong");
            do_release = instance->material != state->default_phong_material;
            break;
        case BRESOURCE_MATERIAL_TYPE_PBR:
            shader_id = shader_system_get_id("Shader.PBRMaterial");
            do_release = instance->material != state->default_pbr_material;
            break;
        case BRESOURCE_MATERIAL_TYPE_LAYERED_PBR:
            shader_id = shader_system_get_id("Shader.LayeredPBRMaterial");
            do_release = instance->material != state->default_layered_material;
            break;
        }

        // Release per-draw resources
        shader_system_shader_per_draw_release(shader_id, instance->per_draw_id, 0, 0);

        // Only release if not a default material
        if (do_release)
            bresource_system_release(state->resource_system, instance->material->base.name);

        instance->material = 0;
        instance->per_draw_id = INVALID_ID;
    }
}

material_instance material_system_get_default_unlit(material_system_state* state)
{
    material_instance instance = {0};
    u32 shader_id = shader_system_get_id("Shader.Unlit");
    // NOTE: No maps for this shader type
    if (!shader_system_shader_per_draw_acquire(shader_id, 0, 0, &instance.per_draw_id))
        BASSERT_MSG(false, "Failed to acquire per-draw renderer resources for default Unlit material. Application cannot continue");

    instance.material = state->default_unlit_material;
    return instance;
}

material_instance material_system_get_default_phong(material_system_state* state)
{
    material_instance instance = {0};
    u32 shader_id = shader_system_get_id("Shader.Phong");
    // NOTE: No maps for this shader type
    if (!shader_system_shader_per_draw_acquire(shader_id, 0, 0, &instance.per_draw_id))
        KASSERT_MSG(false, "Failed to acquire per-draw renderer resources for default Phong material. Application cannot continue");

    instance.material = state->default_phong_material;
    return instance;
}

material_instance material_system_get_default_pbr(material_system_state* state)
{
    material_instance instance = {0};
    // FIXME: use kname instead
    u32 shader_id = shader_system_get_id("Shader.PBRMaterial");
    // NOTE: No maps for this shader type
    if (!shader_system_shader_per_draw_acquire(shader_id, 0, 0, &instance.per_draw_id))
        BASSERT_MSG(false, "Failed to acquire per-draw renderer resources for default PBR material. Application cannot continue");

    instance.material = state->default_pbr_material;
    return instance;
}

material_instance material_system_get_default_layered_pbr(material_system_state* state)
{
    material_instance instance = {0};
    // FIXME: use kname instead
    u32 shader_id = shader_system_get_id("Shader.LayeredPBRMaterial");
    // NOTE: No maps for this shader type
    if (!shader_system_shader_per_draw_acquire(shader_id, 0, 0, &instance.per_draw_id))
        KASSERT_MSG(false, "Failed to acquire per-draw renderer resources for default LayeredPBR material. Application cannot continue");

    instance.material = state->default_layered_material;
    return instance;
}

void material_system_dump(material_system_state* state)
{
    // FIXME: find a way to query this from the bresource system
    /* material_reference* refs = (material_reference*)state_ptr->registered_material_table.memory;
    for (u32 i = 0; i < state_ptr->registered_material_table.element_count; ++i)
    {
        material_reference* r = &refs[i];
        if (r->reference_count > 0 || r->handle != INVALID_ID)
        {
            BTRACE("Found material ref (handle/refCount): (%u/%u)", r->handle, r->reference_count);
            if (r->handle != INVALID_ID)
                BTRACE("Material name: %s", state_ptr->registered_materials[r->handle].name);
        }
    } */
}

static b8 assign_map(material_system_state* state, bresource_texture_map* map, const material_map* config, bname material_name, const bresource_texture* default_tex)
{
    map->filter_minify = config->filter_min;
    map->filter_magnify = config->filter_mag;
    map->repeat_u = config->repeat_u;
    map->repeat_v = config->repeat_v;
    map->repeat_w = config->repeat_w;
    map->mip_levels = 1;
    map->generation = INVALID_ID;

    if (config->texture_name && string_length(config->texture_name) > 0)
    {
        map->texture = texture_system_request(
            bname_create(config->texture_name),
            INVALID_BNAME, // Use the resource from the package where it is first found
            0,             // no listener
            0              // no callback
        );
        if (!map->texture)
        {
            // Use default texture instead if provided
            if (default_tex)
            {
                BWARN("Failed to request material texture '%s'. Using default '%s'", config->texture_name, bname_string_get(default_tex->base.name));
                map->texture = default_tex;
            }
            else
            {
                BERROR("Failed to request material texture '%s', and no default was provided", config->texture_name);
                return false;
            }
        }
    }
    else
    {
        // This is done when a texture is not configured, as opposed to when it is configured and not found (above)
        map->texture = default_tex;
    }

    // Acquire texture map resources
    if (!renderer_bresource_texture_map_resources_acquire(state->renderer, map))
    {
        BERROR("Unable to acquire resources for texture map");
        return false;
    }
    return true;
}

static b8 create_default_standard_material(material_system_state* state)
{
    bresource_material_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_MATERIAL;
    request.material_source_text = "\
        version = 3\
        type = \"standard\"\
        \
        albedo_texture = \"default_albedo\"\
        normal_texture = \"default_normal\"\
        mra_texture = \"default_mra\"\
        emissive_texture = \"default_emissive\"\
        emissive_intensity = 1.0\
        has_transparency = false\
        double_sided = false\
        recieves_shadow = true\
        casts_shadow = true\
        normal_enabled = true\
        ao_enabled = false\
        emissive_enabled = false\
        refraction_enabled = false\
        use_vertex_color_as_albedo = false";

    state->default_pbr_material = (bresource_material*)bresource_system_request(state->resource_system, bname_create("default"), (bresource_request_info*)&request);
    
    u32 shader_id = shader_system_get_id("Shader.PBRMaterial");
    bresource_material* m = state->default_pbr_material;

    bresource_texture_map* maps[PBR_MATERIAL_MAP_COUNT] = {
        &m->albedo_diffuse_map,
        &m->normal_map,
        &m->metallic_roughness_ao_map
        // TODO: emissive
    };

    // Acquire group resources
    if (!shader_system_shader_group_acquire(shader_id, PBR_MATERIAL_MAP_COUNT, maps, &m->group_id))
    {
        BERROR("Unable to acquire group resources for default PBR material");
        return false;
    }

    return true;
}

static b8 create_default_multi_material(material_system_state* state)
{
    bresource_material_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_MATERIAL;
    // FIXME: figure out how the layers should look for this material type
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

    state->default_layered_material = (bresource_material*)bresource_system_request(state->resource_system, bname_create("default_layered"), (bresource_request_info*)&request);

    // TODO: change to layered material shader
    u32 shader_id = shader_system_get_id("Shader.Builtin.Terrain");
    bresource_material* m = state->default_layered_material;

    // NOTE: This is an array that includes 3 maps (albedo, normal, met/roughness/ao) per layer
    bresource_texture_map* maps[LAYERED_PBR_MATERIAL_MAP_COUNT] = {&m->layered_material_map};

    // Acquire group resources
    if (!shader_system_shader_group_acquire(shader_id, LAYERED_PBR_MATERIAL_MAP_COUNT, maps, &m->group_id))
    {
        BERROR("Unable to acquire group resources for default layered PBR material");
        return false;
    }

    return true;
}

static void on_material_system_dump(console_command_context context)
{
    material_system_dump(engine_systems_get()->material_system);
}
