#include "assets/basset_types.h"
#include "core/engine.h"
#include "debug/bassert.h"
#include "defines.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "strings/bname.h"
#include "strings/bstring.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

// The number of channels per PBR material
// i.e. albedo, normal, metallic/roughness/AO combined
#define PBR_MATERIAL_CHANNEL_COUNT 3

typedef struct material_resource_handler_info
{
    bresource_material* typed_resource;
    bresource_handler* handler;
    bresource_material_request_info* request_info;
    basset_material* asset;
} material_resource_handler_info;

static void material_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);

bresource* bresource_handler_material_allocate(void)
{
    return (bresource*)ballocate(sizeof(bresource_material), MEMORY_TAG_RESOURCE);
}

b8 bresource_handler_material_request(bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_material_request requires valid pointers to self and resource");
        return false;
    }
    bresource_material* typed_resource = (bresource_material*)resource;
    bresource_material_request_info* typed_request = (bresource_material_request_info*)info;
    typed_resource->base.state = BRESOURCE_STATE_UNINITIALIZED;
    if (info->assets.base.length != 1)
    {
        BERROR("bresource_handler_material_request requires exactly one asset");
        return false;
    }
    // NOTE: dynamically allocating this so lifetime isn't a concern
    material_resource_handler_info* listener_inst = ballocate(sizeof(material_resource_handler_info), MEMORY_TAG_RESOURCE);
    // Take a copy of the typed request info
    listener_inst->request_info = ballocate(sizeof(bresource_material_request_info), MEMORY_TAG_RESOURCE);
    bcopy_memory(listener_inst->request_info, typed_request, sizeof(bresource_material_request_info));
    listener_inst->typed_resource = typed_resource;
    listener_inst->handler = self;
    listener_inst->asset = 0;
    typed_resource->base.state = BRESOURCE_STATE_INITIALIZED;
    typed_resource->base.state = BRESOURCE_STATE_LOADING;
    bresource_asset_info* asset_info = &info->assets.data[0];
    asset_system_request(
        self->asset_system,
        asset_info->type,
        asset_info->package_name,
        asset_info->asset_name,
        true,
        listener_inst,
        material_basset_on_result,
        0,
        0);
    return true;
}

void bresource_handler_material_release(bresource_handler* self, bresource* resource)
{
}

static b8 process_asset_material_map(bname material_name, basset_material_map* map, bresource_texture_map* target_map)
{
    target_map->repeat_u = map->repeat_u;
    target_map->repeat_v = map->repeat_v;
    target_map->repeat_w = map->repeat_w;
    target_map->filter_minify = map->filter_min;
    target_map->filter_magnify = map->filter_mag;
    target_map->internal_id = 0;
    target_map->generation = INVALID_ID;
    target_map->mip_levels = 0; // TODO: Do we need this?
    if (!renderer_bresource_texture_map_resources_acquire(engine_systems_get()->renderer_system, target_map))
    {
        BERROR("Failed to acquire texture map resources for material '%s', for the '%s' map", material_name, map->name);
        return false;
    }
    target_map->texture = texture_system_request(
        map->image_asset_name,
        map->image_asset_package_name,
        0,
        0);
    return true;
}

typedef enum mra_state
{
    MRA_STATE_UNINITIALIZED,
    MRA_STATE_REQUESTED,
    MRA_STATE_LOADED
} mra_state;

typedef enum mra_indices
{
    MRA_INDEX_METALLIC = 0,
    MRA_INDEX_ROUGHNESS = 1,
    MRA_INDEX_AO = 2
} mra_indices;

typedef struct material_mra_data
{
    mra_state state;
    basset_material_map_channel channel;
    bname image_asset_name;
    bname image_asset_package_name;
    basset_image* asset;
} material_mra_data;

typedef struct material_mra_listener
{
    bresource_texture_map* metallic_roughness_ao_map;
    basset_material_map metallic_roughness_ao_map_config;
    material_mra_data map_assets[3];
    bresource_material* typed_resource;
} material_mra_listener;

static void material_on_metallic_roughness_ao_image_asset_loaded(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        material_mra_listener* listener = (material_mra_listener*)listener_inst;
        // Test for which asset loaded
        for (u32 i = 0; i < 3; ++i)
        {
            material_mra_data* m = &listener->map_assets[i];
            if (m->image_asset_name == asset->name)
            {
                m->state = MRA_STATE_LOADED;
                m->asset = (basset_image*)asset;
                break;
            }
        }
        // Boot if we are waiting on an asset to finish loading
        for (u32 i = 0; i < 3; ++i)
        {
            material_mra_data* m = &listener->map_assets[i];
            if (m->state == MRA_STATE_REQUESTED)
            {
                BTRACE("Still waiting on asset '%s'...", bname_string_get(m->image_asset_name));
                return;
            }
        }
        // This means everything that was request to load has loaded, and combination of asset channel data may begin
        u32 width = U32_MAX, height = U32_MAX;
        u8* pixels = 0;
        u32 pixel_array_size = 0;
        for (u32 i = 0; i < 3; ++i)
        {
            material_mra_data* m = &listener->map_assets[i];
            if (m->state == MRA_STATE_LOADED)
            {
                if (width == U32_MAX || height == U32_MAX)
                {
                    width = m->asset->width;
                    height = m->asset->height;
                    pixel_array_size = sizeof(u8) * width * height * 4;
                    pixels = ballocate(pixel_array_size, MEMORY_TAG_RESOURCE);
                }
                else if (width != m->asset->width || height != m->asset->height)
                {
                    BWARN("All assets for material metallic, roughness and AO maps must be the same resolution. Default data will be used instead");
                    // Use default data instead by releasing the asset and resetting the state
                    asset_system_release(engine_systems_get()->asset_state, m->image_asset_name, m->image_asset_package_name);
                    m->asset = 0;
                    m->state = MRA_STATE_UNINITIALIZED;
                }
            }
            else if (m->state == MRA_STATE_UNINITIALIZED)
            {
                // TODO: Use default data instead
            }
        }

        if (!pixels)
        {
            BERROR("Pixel array not created during asset load for material. This likely means other errors have occurred. Check logs");
            return;
        }

        for (u32 i = 0; i < 3; ++i)
        {
            material_mra_data* m = &listener->map_assets[i];
            if (m->state == MRA_STATE_LOADED)
            {
                u8 offset = 0;
                switch (m->channel)
                {
                default:
                case BASSET_MATERIAL_MAP_CHANNEL_METALLIC:
                    offset = 0;
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_ROUGHNESS:
                    offset = 1;
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_AO:
                    offset = 2;
                    break;
                }

                for (u64 row = 0; row < height; ++row)
                {
                    for (u64 col = 0; col < width; ++col)
                    {
                        u64 index = (row * width) + col;
                        u64 index_bpp = index * 4;
                        pixels[index_bpp + offset] = m->asset->pixels[index_bpp + 0]; // Pull from the red channel
                    }
                }
            }
            else if (m->state == MRA_STATE_UNINITIALIZED)
            {
                // Use default data instead
                u32 offset = 0;
                u8 value = 0;
                switch (m->channel)
                {
                default:
                case BASSET_MATERIAL_MAP_CHANNEL_METALLIC:
                    offset = 0;
                    value = 0; // Default for metallic is black
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_ROUGHNESS:
                    offset = 1;
                    value = 128; // Default for roughness is medium grey
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_AO:
                    offset = 2;
                    value = 255; // Default for AO is white
                    break;
                }

                for (u64 row = 0; row < height; ++row)
                {
                    for (u64 col = 0; col < width; ++col)
                    {
                        u64 index = (row * width) + col;
                        u64 index_bpp = index * 4;
                        pixels[index_bpp + offset] = value;
                    }
                }
            }
        }

        if (!listener->metallic_roughness_ao_map->texture)
        {
            bresource_texture_map* target_map = listener->metallic_roughness_ao_map;
            target_map->repeat_u = listener->metallic_roughness_ao_map_config.repeat_u;
            target_map->repeat_v = listener->metallic_roughness_ao_map_config.repeat_v;
            target_map->repeat_w = listener->metallic_roughness_ao_map_config.repeat_w;
            target_map->filter_minify = listener->metallic_roughness_ao_map_config.filter_min;
            target_map->filter_magnify = listener->metallic_roughness_ao_map_config.filter_mag;
            target_map->internal_id = 0;
            target_map->generation = INVALID_ID;
            target_map->mip_levels = 0; // TODO: Do we need this?

            // Setup texture map resources
            if (!renderer_bresource_texture_map_resources_acquire(engine_systems_get()->renderer_system, target_map))
            {
                BERROR("Failed to acquire texture map resources for material '%s', for the '%s' map", bname_string_get(listener->typed_resource->base.name), listener->metallic_roughness_ao_map_config.name);
                return;
            }

            const char* map_name = string_format("%s_metallic_roughness_ao_generated", listener->typed_resource->base.name);
            listener->typed_resource->metallic_roughness_ao_map.texture = texture_system_request_writeable(
                bname_create(map_name),
                width, height, BRESOURCE_TEXTURE_FORMAT_RGBA8, false, false);
            string_free(map_name);

            if (!texture_system_write_data((bresource_texture*)listener->metallic_roughness_ao_map->texture, 0, pixel_array_size, pixels))
            {
                BERROR("Failed to upload combined texture data for material resource '%s'", bname_string_get(listener->typed_resource->base.name));
                return;
            }

            BTRACE("Successfully uploaded combined texture data for material resource '%s'", bname_string_get(listener->typed_resource->base.name));
        }
    }
    else
    {
        BERROR("Asset failed to load. See logs for details");
    }
}

static void material_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    material_resource_handler_info* listener = (material_resource_handler_info*)listener_inst;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        // Save off the asset pointer to the array
        listener->asset = (basset_material*)asset;
        // TODO: Need to think about hot-reloading here, and how/where listening should happen. Maybe in the resource system?
        listener->typed_resource->type = BRESOURCE_MATERIAL_TYPE_UNKNOWN;
        // Examine the material asset's type
        switch (listener->asset->type)
        {
        default:
        case BMATERIAL_TYPE_UNKNOWN:
        case BMATERIAL_TYPE_COUNT:
            BERROR("Unknown material type cannot be processed");
            return;
        case BMATERIAL_TYPE_UNLIT:
            for (u32 i = 0; i < listener->asset->property_count; ++i)
            {
                basset_material_property* prop = &listener->asset->properties[i];
                if (prop->name == bname_create("diffuse_color"))
                    listener->typed_resource->diffuse_color = prop->value.v4;
            }
            for (u32 i = 0; i < listener->asset->map_count; ++i)
            {
                basset_material_map* map = &listener->asset->maps[i];
                if (map->channel == BASSET_MATERIAL_MAP_CHANNEL_DIFFUSE)
                {
                    if (!process_asset_material_map(listener->typed_resource->base.name, map, &listener->typed_resource->albedo_diffuse_map))
                        BERROR("Failed to process material map. See logs for details");
                }
                else
                {
                    BERROR("An unlit material does not use a map of '%u' type (name='%s'). Skipping...", map->channel, map->name);
                    continue;
                }
            }
            break;
        case BMATERIAL_TYPE_PHONG:
            for (u32 i = 0; i < listener->asset->property_count; ++i)
            {
                basset_material_property* prop = &listener->asset->properties[i];
                if (prop->name == bname_create("diffuse_color"))
                {
                    listener->typed_resource->diffuse_color = prop->value.v4;
                }
                else if (prop->name == bname_create("specular_strength"))
                {
                    listener->typed_resource->specular_strength = prop->value.f32;
                }
                else
                {
                    BWARN("Property '%s' for material '%s' not recognized. Skipping...", bname_string_get(prop->name), bname_string_get(listener->typed_resource->base.name));
                }
            }

            for (u32 i = 0; i < listener->asset->map_count; ++i)
            {
                basset_material_map* map = &listener->asset->maps[i];
                switch (map->channel)
                {
                case BASSET_MATERIAL_MAP_CHANNEL_DIFFUSE:
                    if (!process_asset_material_map(listener->typed_resource->base.name, map, &listener->typed_resource->albedo_diffuse_map))
                        BERROR("Failed to process material map '%s'. See logs for details", map->name);
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_NORMAL:
                    if (!process_asset_material_map(listener->typed_resource->base.name, map, &listener->typed_resource->normal_map))
                        BERROR("Failed to process material map '%s'. See logs for details", map->name);
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_SPECULAR:
                    if (!process_asset_material_map(listener->typed_resource->base.name, map, &listener->typed_resource->specular_map))
                        BERROR("Failed to process material map '%s'. See logs for details", map->name);
                default:
                    BERROR("An unlit material does not use a map of '%u' type (name='%s'). Skipping...", map->channel, map->name);
                    break;
                }
            }
            break;
        case BMATERIAL_TYPE_PBR:
        {
            for (u32 i = 0; i < listener->asset->property_count; ++i)
            {
                basset_material_property* prop = &listener->asset->properties[i];
                /* if (prop->name == bname_create("diffuse_color")) {
                    listener->typed_resource->diffuse_color = prop->value.v4;
                } else if (prop->name == bname_create("specular_strength")) {
                    listener->typed_resource->specular_strength = prop->value.f32;
                } else { */
                BWARN("Property '%s' for material '%s' not recognized. Skipping...", bname_string_get(prop->name), bname_string_get(listener->typed_resource->base.name));
                /* } */
            }

            material_mra_listener* listener_inst = 0;
            for (u32 i = 0; i < listener->asset->map_count; ++i)
            {
                basset_material_map* map = &listener->asset->maps[i];
                switch (map->channel)
                {
                case BASSET_MATERIAL_MAP_CHANNEL_ALBEDO:
                    if (!process_asset_material_map(listener->typed_resource->base.name, map, &listener->typed_resource->albedo_diffuse_map))
                        BERROR("Failed to process material map '%s'. See logs for details", map->name);
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_NORMAL:
                    if (!process_asset_material_map(listener->typed_resource->base.name, map, &listener->typed_resource->normal_map))
                        BERROR("Failed to process material map '%s'. See logs for details", map->name);
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_EMISSIVE:
                    if (!process_asset_material_map(listener->typed_resource->base.name, map, &listener->typed_resource->emissive_map))
                        BERROR("Failed to process material map '%s'. See logs for details", map->name);
                    break;
                case BASSET_MATERIAL_MAP_CHANNEL_METALLIC:
                case BASSET_MATERIAL_MAP_CHANNEL_ROUGHNESS:
                case BASSET_MATERIAL_MAP_CHANNEL_AO:
                {
                    if (!listener_inst)
                    {
                        listener_inst = ballocate(sizeof(material_mra_listener), MEMORY_TAG_RESOURCE);
                        listener_inst->metallic_roughness_ao_map = &listener->typed_resource->metallic_roughness_ao_map;
                        listener_inst->metallic_roughness_ao_map_config = listener->asset->maps[i];
                    }
                    u32 index;
                    switch (map->channel)
                    {
                    default:
                    case BASSET_MATERIAL_MAP_CHANNEL_METALLIC:
                        index = MRA_INDEX_METALLIC;
                        break;
                    case BASSET_MATERIAL_MAP_CHANNEL_ROUGHNESS:
                        index = MRA_INDEX_ROUGHNESS;
                        break;
                    case BASSET_MATERIAL_MAP_CHANNEL_AO:
                        index = MRA_INDEX_AO;
                        break;
                    }

                    material_mra_data* m = &listener_inst->map_assets[index];
                    m->state = MRA_STATE_REQUESTED;
                    m->channel = map->channel;
                    m->image_asset_package_name = map->image_asset_package_name;
                    m->image_asset_name = map->image_asset_name;
                    break;
                }
                default:
                    BERROR("An unlit material does not use a map of '%u' type (name='%s'). Skipping...", map->channel, map->name);
                    break;
                }
                // Perform the actual asset requests for the "combined" metallic/roughness/ao map
                // This must be done after flags are flipped because this process is asynchronous and can otherwise cause a race condition
                if (listener_inst)
                {
                    for (u32 i = 0; i < PBR_MATERIAL_CHANNEL_COUNT; ++i)
                    {
                        material_mra_data* m = &listener_inst->map_assets[i];
                        // Request a writeable texture to use as the final combined metallic/roughness/ao map
                        asset_system_request(
                            engine_systems_get()->asset_state,
                            BASSET_TYPE_IMAGE,
                            m->image_asset_package_name,
                            m->image_asset_name,
                            true,
                            listener_inst,
                            material_on_metallic_roughness_ao_image_asset_loaded,
                            0,
                            0);
                    }
                }
            }
            
            // Acquire instance resources from the PBR shader
            bresource_material* m = listener->typed_resource;
            bresource_texture_map* material_maps[PBR_MATERIAL_CHANNEL_COUNT] = {&m->albedo_diffuse_map,
                                                                                &m->normal_map,
                                                                                &m->metallic_roughness_ao_map};

            u32 pbr_shader_id = shader_system_get_id("Shader.PBRMaterial");
            if (!shader_system_shader_instance_acquire(pbr_shader_id, PBR_MATERIAL_CHANNEL_COUNT, material_maps, &m->instance_id))
                BASSERT_MSG(false, "Failed to acquire renderer resources for default PBR material. Application cannot continue");
        } break;
        case BMATERIAL_TYPE_CUSTOM:
            BASSERT_MSG(false, "custom material type not yet supported");
            return;
        case BMATERIAL_TYPE_PBR_WATER:
            BASSERT_MSG(false, "water material type not yet supported");
            return;
        }

        listener->typed_resource->base.state = BRESOURCE_STATE_LOADED;
    }
    else
    {
        BERROR("Failed to load a required asset for material resource '%s'. Resource may not appear correctly when rendered", bname_string_get(listener->typed_resource->base.name));
    }

    // Destroy the request
    array_bresource_asset_info_destroy(&listener->request_info->base.assets);
    bfree(listener->request_info, sizeof(bresource_material_request_info), MEMORY_TAG_RESOURCE);
    // Free the listener itself
    bfree(listener, sizeof(material_resource_handler_info), MEMORY_TAG_RESOURCE);
}
