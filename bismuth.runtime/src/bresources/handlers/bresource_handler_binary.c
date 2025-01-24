#include "bresource_handler_binary.h"

#include <assets/basset_types.h>
#include <defines.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <strings/bname.h>

#include "containers/darray.h"
#include "core/event.h"
#include "bresources/bresource_types.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"

static void binary_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);
static void asset_to_resource(const basset_binary* asset, bresource_binary* out_binary);
static void binary_basset_on_hot_reload(asset_request_result result, const struct basset* asset, void* listener_inst);

bresource* bresource_handler_binary_allocate(void)
{
    return (bresource*)BALLOC_TYPE(bresource_binary, MEMORY_TAG_RESOURCE);
}

b8 bresource_handler_binary_request(bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_binary_request requires valid pointers to self and resource");
        return false;
    }

    bresource_binary* typed_resource = (bresource_binary*)resource;
    /* typed_resource->base.state = BRESOURCE_STATE_UNINITIALIZED; */
    /* typed_resource->base.state = BRESOURCE_STATE_INITIALIZED; */
    // Straight to loading state
    typed_resource->base.state = BRESOURCE_STATE_LOADING;

    bresource_asset_info* asset_info = &info->assets.data[0];

    asset_request_info request_info = {0};
    request_info.type = asset_info->type;
    request_info.asset_name = asset_info->asset_name;
    request_info.package_name = asset_info->package_name;
    request_info.auto_release = true;
    request_info.listener_inst = typed_resource;
    request_info.callback = binary_basset_on_result;
    request_info.synchronous = true;
    request_info.hot_reload_callback = binary_basset_on_hot_reload;
    request_info.hot_reload_context = typed_resource;
    request_info.import_params_size = 0;
    request_info.import_params = 0;
    asset_system_request(self->asset_system, request_info);

    return true;
}

void bresource_handler_binary_release(bresource_handler* self, bresource* resource)
{
    if (resource)
    {
        bresource_binary* typed_resource = (bresource_binary*)resource;

        if (typed_resource->bytes && typed_resource->size)
        {
            bfree(typed_resource->bytes, typed_resource->size, MEMORY_TAG_RESOURCE);
            typed_resource->bytes = 0;
            typed_resource->size = 0;
        }

        BFREE_TYPE(typed_resource, bresource_binary, MEMORY_TAG_RESOURCE);
    }
}

static void binary_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    bresource_binary* typed_resource = (bresource_binary*)listener_inst;
    basset_binary* typed_asset = (basset_binary*)asset;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        typed_resource->bytes = ballocate(typed_asset->size, MEMORY_TAG_RESOURCE);
        bcopy_memory(typed_resource->bytes, typed_asset->content, typed_asset->size);
        if (asset->file_watch_id != INVALID_ID)
        {
            if (!typed_resource->base.asset_file_watch_ids)
                typed_resource->base.asset_file_watch_ids = darray_create(u32);
            darray_push(typed_resource->base.asset_file_watch_ids, asset->file_watch_id);
        }
        typed_resource->base.generation++;
    }
    else
    {
        BERROR("Failed to load a required asset for binary resource '%s'", bname_string_get(typed_resource->base.name));
    }
}

static void binary_basset_on_hot_reload(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    bresource_binary* typed_resource = (bresource_binary*)listener_inst;
    basset_binary* typed_asset = (basset_binary*)asset;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        // Free the old binary data and replace it with the new data from the asset
        if (typed_resource->bytes)
            bfree(typed_resource->bytes, typed_resource->size, MEMORY_TAG_RESOURCE);
        typed_resource->bytes = ballocate(typed_asset->size, MEMORY_TAG_RESOURCE);
        bcopy_memory(typed_resource->bytes, typed_asset->content, typed_asset->size);

        typed_resource->base.generation++;

        // Fire off an event that this asset has hot-reloaded
        // Sender should be a pointer to the resource itself
        event_context context = {0};
        context.data.u32[0] = asset->file_watch_id;
        event_fire(EVENT_CODE_RESOURCE_HOT_RELOADED, (void*)typed_resource, context);
    }
    else
    {
        BWARN("Hot reload was triggered for binary resource '%s', but was unsuccessful. See logs for details", bname_string_get(typed_resource->base.name));
    }
}
