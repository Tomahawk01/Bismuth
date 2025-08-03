#include "bresource_handler_system_font.h"

#include <assets/basset_types.h>
#include <defines.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <serializers/basset_system_font_serializer.h>
#include <strings/bname.h>

#include "core/engine.h"
#include "bresources/bresource_types.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"

static void system_font_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);
static void asset_to_resource(const basset_system_font* asset, bresource_system_font* out_system_font);

b8 bresource_handler_system_font_request(bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_system_font_request requires valid pointers to self and resource");
        return false;
    }

    bresource_system_font* typed_resource = (bresource_system_font*)resource;
    typed_resource->base.state = BRESOURCE_STATE_UNINITIALIZED;

    if (info->assets.base.length < 1)
    {
        BERROR("bresource_handler_system_font_request requires exactly one asset");
        return false;
    }

    typed_resource->base.state = BRESOURCE_STATE_INITIALIZED;
    typed_resource->base.state = BRESOURCE_STATE_LOADING;

    // Load the asset from disk synchronously.
    basset_system_font* asset = asset_system_request_system_font_from_package_sync(
        engine_systems_get()->asset_state,
        bname_string_get(info->assets.data[0].package_name),
        bname_string_get(info->assets.data[0].asset_name));

    bresource_system_font* out_system_font = typed_resource;

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

    // Destroy the request
    array_bresource_asset_info_destroy((array_bresource_asset_info*)&info->assets);

    return true;
}

void bresource_handler_system_font_release(bresource_handler* self, bresource* resource)
{
    if (resource)
    {
        bresource_system_font* typed_resource = (bresource_system_font*)resource;

        if (typed_resource->faces && typed_resource->face_count)
            BFREE_TYPE_CARRAY(typed_resource->faces, bname, typed_resource->face_count);
    }
}
