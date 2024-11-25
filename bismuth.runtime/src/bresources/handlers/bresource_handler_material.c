#include "assets/basset_types.h"
#include "defines.h"
#include "basset_bresource_utils.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "serializers/basset_material_serializer.h"
#include "strings/bname.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"

typedef struct material_resource_handler_info
{
    bresource_material* typed_resource;
    bresource_handler* handler;
    bresource_material_request_info* request_info;
    basset_material* asset;
} material_resource_handler_info;

static void material_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);
static void asset_to_resource(const basset_material* asset, bresource_material* out_material);

bresource* bresource_handler_material_allocate(void)
{
    return (bresource*)BALLOC_TYPE(bresource_material, MEMORY_TAG_RESOURCE);
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
        if (info->assets.base.length == 0 && typed_request->material_source_text)
        {
            // Deserialize material asset from provided source
            basset material_from_source = {0};
            if (!basset_material_deserialize(typed_request->material_source_text, &material_from_source))
            {
                BERROR("Failed to deserialize material from direct source upon resource request");
                return false;
            }
            asset_to_resource((basset_material*)&material_from_source, typed_resource);
            return true;
        }
        else
        {
            BERROR("bresource_handler_material_request requires exactly one asset OR zero assets and material source text");
            return false;
        }
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
    if (resource)
    {
        bresource_material* typed_resource = (bresource_material*)resource;

        if (typed_resource->custom_sampler_count && typed_resource->custom_samplers)
            BFREE_TYPE_CARRAY(typed_resource->custom_samplers, basset_material_sampler, typed_resource->custom_sampler_count);
    }

    BFREE_TYPE(typed_resource, bresource_material, MEMORY_TAG_RESOURCE);
}

static void material_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    material_resource_handler_info* listener = (material_resource_handler_info*)listener_inst;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        // Save off the asset pointer to the array
        listener->asset = (basset_material*)asset;
        
        asset_to_resource(listener->asset, listener->typed_resource);
    }
    else
    {
        BERROR("Failed to load a required asset for material resource '%s'. Resource may not appear correctly when rendered", bname_string_get(listener->typed_resource->base.name));
    }

    // Destroy the request
    array_kresource_asset_info_destroy(&listener->request_info->base.assets);
    bfree(listener->request_info, sizeof(bresource_material_request_info), MEMORY_TAG_RESOURCE);
    // Free the listener itself
    bfree(listener, sizeof(material_resource_handler_info), MEMORY_TAG_RESOURCE);
}

static void asset_to_resource(const basset_material* asset, bresource_material* out_material)
{
    // Take a copy of all of the asset properties
    out_material->type = basset_material_type_to_bresource(asset->type);
    out_material->model = basset_material_model_to_bresource(asset->model);
                
    out_material->has_transparency = asset->has_transparency;
    out_material->double_sided = asset->double_sided;
    out_material->recieves_shadow = asset->recieves_shadow;
    out_material->casts_shadow = asset->casts_shadow;
    out_material->use_vertex_color_as_base_color = asset->use_vertex_color_as_base_color;

    out_material->custom_shader_name = asset->custom_shader_name;
            
    out_material->base_color = asset->base_color;
    out_material->base_color_map = basset_material_texture_to_bresource(asset->base_color_map);

    out_material->normal_enabled = asset->normal_enabled;
    out_material->normal = asset->normal;
    out_material->normal_map = basset_material_texture_to_bresource(asset->normal_map);

    out_material->metallic = asset->metallic;
    out_material->metallic_map = basset_material_texture_to_bresource(asset->metallic_map);
    out_material->metallic_map_source_channel = basset_material_tex_map_channel_to_bresource(asset->metallic_map_source_channel);

    out_material->roughness = asset->roughness;
    out_material->roughness_map = basset_material_texture_to_bresource(asset->roughness_map);
    out_material->roughness_map_source_channel = basset_material_tex_map_channel_to_bresource(asset->roughness_map_source_channel);

    out_material->ambient_occlusion_enabled = asset->ambient_occlusion_enabled;
    out_material->ambient_occlusion = asset->ambient_occlusion;
    out_material->ambient_occlusion_map = basset_material_texture_to_bresource(asset->ambient_occlusion_map);
    out_material->ambient_occlusion_map_source_channel = basset_material_tex_map_channel_to_bresource(asset->ambient_occlusion_map_source_channel);

    out_material->mra = asset->mra;
    out_material->mra_map = basset_material_texture_to_bresource(asset->mra_map);
    out_material->use_mra = asset->use_mra;

    out_material->emissive_enabled = asset->emissive_enabled;
    out_material->emissive = asset->emissive;
    out_material->emissive_map = basset_material_texture_to_bresource(asset->emissive_map);

    out_material->custom_sampler_count = asset->custom_sampler_count;
    BALLOC_TYPE_CARRAY(basset_material_sampler, out_material->custom_sampler_count);
    BCOPY_TYPE_CARRAY(
        out_material->custom_samplers,
        asset->custom_samplers,
        basset_material_sampler,
        out_material->custom_sampler_count);

    out_material->base.state = BRESOURCE_STATE_LOADED;
}
