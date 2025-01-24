#include "bresource_handler_audio.h"
#include "assets/basset_types.h"
#include "containers/array.h"
#include "core/engine.h"
#include "debug/bassert.h"
#include "bresources/bresource_types.h"
#include "bresources/bresource_utils.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "strings/bname.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"

// TODO: move this to basset_types?
ARRAY_TYPE_NAMED(const basset_image*, bimage_ptr);

typedef struct audio_resource_handler_info
{
    bresource_audio* typed_resource;
    bresource_handler* handler;
    bresource_audio_request_info* request_info;
    array_bimage_ptr assets;
    u32 loaded_count;
} audio_resource_handler_info;

static void audio_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);

bresource* bresource_handler_audio_allocate(void)
{
    return (bresource*)ballocate(sizeof(bresource_audio), MEMORY_TAG_RESOURCE);
}

b8 bresource_handler_audio_request(struct bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_audio_request requires valid pointers to self and resource");
        return false;
    }

    bresource_audio* typed_resource = (bresource_audio*)resource;
    bresource_audio_request_info* typed_request = (bresource_audio_request_info*)info;
    struct renderer_system_state* renderer = engine_systems_get()->renderer_system;

    b8 assets_required = true;

    // NOTE: dynamically allocating this so lifetime isn't a concern
    audio_resource_handler_info* listener_inst = ballocate(sizeof(audio_resource_handler_info), MEMORY_TAG_RESOURCE);
    // Take a copy of the typed request info
    listener_inst->request_info = ballocate(sizeof(bresource_audio_request_info), MEMORY_TAG_RESOURCE);
    kcopy_memory(listener_inst->request_info, typed_request, sizeof(bresource_audio_request_info));
    listener_inst->typed_resource = typed_resource;
    listener_inst->handler = self;
    listener_inst->loaded_count = 0;
    if (assets_required)
        listener_inst->assets = array_bimage_ptr_create(info->assets.base.length);

    if (info->assets.base.length != 1)
    {
        BERROR("bresource_handler_audio requires exactly one asset. Request failed");
        return false;
    }

    // Load the asset
    bresource_asset_info* asset_info = &info->assets.data[0];
    if (asset_info->type == BASSET_TYPE_AUDIO)
    {
        asset_request_info request_info = {0};
        request_info.type = asset_info->type;
        request_info.asset_name = asset_info->asset_name;
        request_info.package_name = asset_info->package_name;
        request_info.auto_release = true;
        request_info.listener_inst = listener_inst;
        request_info.callback = audio_basset_on_result;
        request_info.synchronous = false;
        request_info.hot_reload_callback = 0;
        request_info.hot_reload_context = 0;
        request_info.import_params_size = 0;
        request_info.import_params = 0;
        asset_system_request(self->asset_system, request_info);
    }
    else
    {
        BERROR("bresource_handler_audio asset must be of audio type");
        return false;
    }

    return true;
}

void bresource_handler_audio_release(struct bresource_handler* self, bresource* resource)
{
    if (resource)
    {
        if (resource->type != BRESOURCE_TYPE_AUDIO)
        {
            BERROR("Attempted to release non-audio resource '%s' via audio resource handler. Resource not released");
            return;
        }

        bresource_audio* t = (bresource_audio*)resource;
        if (t->pcm_data_size && t->pcm_data)
            bfree(t->pcm_data, t->pcm_data_size, MEMORY_TAG_AUDIO);

        // TODO: release backend data

        bfree(resource, sizeof(bresource_audio), MEMORY_TAG_RESOURCE);
    }
}

static void audio_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    audio_resource_handler_info* listener = (audio_resource_handler_info*)listener_inst;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        // Release the asset reference as we are done with it
        asset_system_release(engine_systems_get()->asset_state, asset->name, asset->package_name);
    }
    else
    {
        BERROR("Failed to load a required asset for audio resource '%s'. Resource may not work correctly when used", bname_string_get(listener->typed_resource->base.name));
    }

destroy_request:
    // Destroy the request
    array_bimage_ptr_destroy(&listener->assets);
    array_bresource_asset_info_destroy(&listener->request_info->base.assets);
    bfree(listener->request_info, sizeof(bresource_audio_request_info), MEMORY_TAG_RESOURCE);
    // Free the listener itself
    bfree(listener, sizeof(audio_resource_handler_info), MEMORY_TAG_RESOURCE);
}
