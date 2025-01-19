#include "bresource_handler_system_font.h"

#include <assets/basset_types.h>
#include <defines.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <serializers/basset_system_font_serializer.h>
#include <strings/bname.h>

#include "bresources/bresource_types.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"

typedef struct system_font_resource_handler_info
{
    bresource_system_font* typed_resource;
    bresource_handler* handler;
    bresource_system_font_request_info* request_info;
    basset_system_font* asset;
} system_font_resource_handler_info;

static void system_font_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);
static void asset_to_resource(const basset_system_font* asset, bresource_system_font* out_system_font);

bresource* bresource_handler_system_font_allocate(void)
{
    return (bresource*)BALLOC_TYPE(bresource_system_font, MEMORY_TAG_RESOURCE);
}

b8 bresource_handler_system_font_request(bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_system_font_request requires valid pointers to self and resource");
        return false;
    }

    bresource_system_font* typed_resource = (bresource_system_font*)resource;
    bresource_system_font_request_info* typed_request = (bresource_system_font_request_info*)info;
    typed_resource->base.state = BRESOURCE_STATE_UNINITIALIZED;

    if (info->assets.base.length < 1)
    {
        BERROR("bresource_handler_system_font_request requires exactly one asset");
        return false;
    }

    // NOTE: dynamically allocating this so lifetime isn't a concern
    system_font_resource_handler_info* listener_inst = ballocate(sizeof(system_font_resource_handler_info), MEMORY_TAG_RESOURCE);
    // Take a copy of the typed request info
    listener_inst->request_info = ballocate(sizeof(bresource_system_font_request_info), MEMORY_TAG_RESOURCE);
    bcopy_memory(listener_inst->request_info, typed_request, sizeof(bresource_system_font_request_info));
    listener_inst->typed_resource = typed_resource;
    listener_inst->handler = self;
    listener_inst->asset = 0;

    typed_resource->base.state = BRESOURCE_STATE_INITIALIZED;

    typed_resource->base.state = BRESOURCE_STATE_LOADING;

    bresource_asset_info* asset_info = &info->assets.data[0];

    asset_request_info request_info = {0};
    request_info.type = asset_info->type;
    request_info.asset_name = asset_info->asset_name;
    request_info.package_name = asset_info->package_name;
    request_info.auto_release = true;
    request_info.listener_inst = listener_inst;
    request_info.callback = system_font_basset_on_result;
    request_info.synchronous = typed_request->base.synchronous;
    request_info.hot_reload_callback = 0;
    request_info.hot_reload_context = 0;
    request_info.import_params_size = 0;
    request_info.import_params = 0;
    asset_system_request(self->asset_system, request_info);

    return true;
}

void bresource_handler_system_font_release(bresource_handler* self, bresource* resource)
{
    if (resource)
    {
        bresource_system_font* typed_resource = (bresource_system_font*)resource;

        if (typed_resource->faces && typed_resource->face_count)
        {
            BFREE_TYPE_CARRAY(typed_resource->faces, bname, typed_resource->face_count);
        }

        bzero_memory(typed_resource, sizeof(bresource_system_font));
    }
}

static void system_font_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    system_font_resource_handler_info* listener = (system_font_resource_handler_info*)listener_inst;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        // Save off the asset pointer to the array
        listener->asset = (basset_system_font*)asset;
        asset_to_resource(listener->asset, listener->typed_resource);

        // Release the asset
        asset_system_release(listener->handler->asset_system, asset->name, asset->package_name);
    }
    else
    {
        BERROR("Failed to load a required asset for system_font resource '%s'. Resource may not appear correctly when rendered", bname_string_get(listener->typed_resource->base.name));
    }

    // Destroy the request
    array_bresource_asset_info_destroy(&listener->request_info->base.assets);
    bfree(listener->request_info, sizeof(bresource_system_font_request_info), MEMORY_TAG_RESOURCE);
    // Free the listener itself
    bfree(listener, sizeof(system_font_resource_handler_info), MEMORY_TAG_RESOURCE);
}

static void asset_to_resource(const basset_system_font* asset, bresource_system_font* out_system_font)
{
    // Take a copy of all of the asset properties
    out_system_font->ttf_asset_name = asset->ttf_asset_name;
    out_system_font->ttf_asset_package_name = asset->ttf_asset_package_name;
    out_system_font->face_count = asset->face_count;
    out_system_font->faces = BALLOC_TYPE_CARRAY(bname, out_system_font->face_count);
    BCOPY_TYPE_CARRAY(out_system_font->faces, asset->faces, bname, out_system_font->face_count);

    // NOTE: The binary should also have been loaded by this point. Take a copy of it
    out_system_font->font_binary_size = asset->font_binary_size;
    out_system_font->font_binary = ballocate(out_system_font->font_binary_size, MEMORY_TAG_RESOURCE);
    bcopy_memory(out_system_font->font_binary, asset->font_binary, out_system_font->font_binary_size);
    
    out_system_font->base.state = BRESOURCE_STATE_LOADED;
}
