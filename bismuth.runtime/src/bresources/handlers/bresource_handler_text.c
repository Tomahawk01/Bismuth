#include "bresource_handler_text.h"

#include <assets/basset_types.h>
#include <defines.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <strings/bname.h>

#include "containers/darray.h"
#include "core/event.h"
#include "bresources/bresource_types.h"
#include "strings/bstring.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"
#include "core/engine.h"

static void text_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);
static void text_basset_on_hot_reload(asset_request_result result, const struct basset* asset, void* listener_inst);

b8 bresource_handler_text_request(bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_text_request requires valid pointers to self and resource");
        return false;
    }

    bresource_text* typed_resource = (bresource_text*)resource;
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
    request_info.callback = text_basset_on_result;
    request_info.synchronous = true;
    request_info.import_params_size = 0;
    request_info.import_params = 0;
    asset_system_request(self->asset_system, request_info);

    return true;
}

void bresource_handler_text_release(bresource_handler* self, bresource* resource)
{
    if (resource)
    {
        bresource_text* typed_resource = (bresource_text*)resource;

        if (typed_resource->text)
        {
            string_free(typed_resource->text);
            typed_resource->text = 0;
        }
    }
}

BAPI b8 bresource_handler_text_handle_hot_reload(struct bresource_handler* self, bresource* resource, basset* asset, u32 file_watch_id)
{
    if (resource && asset)
    {
        bresource_text* typed_resource = (bresource_text*)resource;
        basset_text* typed_asset = (basset_text*)asset;

        if (typed_resource->text)
        {
            string_free(typed_resource->text);
            typed_resource->text = 0;
        }

        typed_resource->text = string_duplicate(typed_asset->content);

        return true;
    }

    return false;
}

static void text_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    bresource_text* typed_resource = (bresource_text*)listener_inst;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        typed_resource->text = string_duplicate(((basset_text*)asset)->content);
        typed_resource->asset_file_watch_id = asset->file_watch_id;
        typed_resource->base.generation++;

        bresource_system_register_for_hot_reload(engine_systems_get()->bresource_state, (bresource*)typed_resource, asset->file_watch_id);
    }
    else
    {
        BERROR("Failed to load a required asset for text resource '%s'", bname_string_get(typed_resource->base.name));
    }
}
