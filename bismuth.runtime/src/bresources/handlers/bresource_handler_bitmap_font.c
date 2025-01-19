#include "bresource_handler_bitmap_font.h"

#include <assets/basset_types.h>
#include <defines.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <serializers/basset_bitmap_font_serializer.h>
#include <strings/bname.h>

#include "bresources/bresource_types.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"

typedef struct bitmap_font_resource_handler_info
{
    bresource_bitmap_font* typed_resource;
    bresource_handler* handler;
    bresource_bitmap_font_request_info* request_info;
    basset_bitmap_font* asset;
} bitmap_font_resource_handler_info;

static void bitmap_font_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);
static void asset_to_resource(const basset_bitmap_font* asset, bresource_bitmap_font* out_bitmap_font);

bresource* bresource_handler_bitmap_font_allocate(void)
{
    return (bresource*)BALLOC_TYPE(bresource_bitmap_font, MEMORY_TAG_RESOURCE);
}

b8 bresource_handler_bitmap_font_request(bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_bitmap_font_request requires valid pointers to self and resource");
        return false;
    }

    bresource_bitmap_font* typed_resource = (bresource_bitmap_font*)resource;
    bresource_bitmap_font_request_info* typed_request = (bresource_bitmap_font_request_info*)info;
    typed_resource->base.state = BRESOURCE_STATE_UNINITIALIZED;

    if (info->assets.base.length < 1)
    {
        BERROR("bresource_handler_bitmap_font_request requires exactly one asset");
        return false;
    }

    // NOTE: dynamically allocating this so lifetime isn't a concern
    bitmap_font_resource_handler_info* listener_inst = ballocate(sizeof(bitmap_font_resource_handler_info), MEMORY_TAG_RESOURCE);
    // Take a copy of the typed request info
    listener_inst->request_info = ballocate(sizeof(bresource_bitmap_font_request_info), MEMORY_TAG_RESOURCE);
    kcopy_memory(listener_inst->request_info, typed_request, sizeof(bresource_bitmap_font_request_info));
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
    request_info.callback = bitmap_font_basset_on_result;
    request_info.synchronous = typed_request->base.synchronous;
    request_info.hot_reload_callback = 0;
    request_info.hot_reload_context = 0;
    request_info.import_params_size = 0;
    request_info.import_params = 0;
    asset_system_request(self->asset_system, request_info);

    return true;
}

void bresource_handler_bitmap_font_release(bresource_handler* self, bresource* resource)
{
    if (resource)
    {
        bresource_bitmap_font* typed_resource = (bresource_bitmap_font*)resource;

        array_font_glyph_destroy(&typed_resource->glyphs);
        array_font_kerning_destroy(&typed_resource->kernings);
        array_font_page_destroy(&typed_resource->pages);
    }
}

static void bitmap_font_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    bitmap_font_resource_handler_info* listener = (bitmap_font_resource_handler_info*)listener_inst;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        // Save off the asset pointer to the array
        listener->asset = (basset_bitmap_font*)asset;
        asset_to_resource(listener->asset, listener->typed_resource);
    }
    else
    {
        BERROR("Failed to load a required asset for bitmap_font resource '%s'. Resource may not appear correctly when rendered", bname_string_get(listener->typed_resource->base.name));
    }

    // Destroy the request
    array_bresource_asset_info_destroy(&listener->request_info->base.assets);
    bfree(listener->request_info, sizeof(bresource_bitmap_font_request_info), MEMORY_TAG_RESOURCE);
    // Free the listener itself
    bfree(listener, sizeof(bitmap_font_resource_handler_info), MEMORY_TAG_RESOURCE);
}

static void asset_to_resource(const basset_bitmap_font* asset, bresource_bitmap_font* out_bitmap_font)
{
    // Take a copy of all of the asset properties
    out_bitmap_font->size = asset->size;
    out_bitmap_font->face = asset->face;
    out_bitmap_font->baseline = asset->baseline;
    out_bitmap_font->line_height = asset->line_height;
    out_bitmap_font->atlas_size_x = asset->atlas_size_x;
    out_bitmap_font->atlas_size_y = asset->atlas_size_y;

    // Glyphs
    if (asset->glyphs.base.length)
    {
        out_bitmap_font->glyphs = array_font_glyph_create(asset->glyphs.base.length);
        for (u32 i = 0; i < asset->glyphs.base.length; ++i)
        {
            basset_bitmap_font_glyph* src = &asset->glyphs.data[i];
            font_glyph* dest = &out_bitmap_font->glyphs.data[i];

            dest->x = src->x;
            dest->y = src->y;
            dest->width = src->width;
            dest->height = src->height;
            dest->x_offset = src->x_offset;
            dest->y_offset = src->y_offset;
            dest->x_advance = src->x_advance;
            dest->codepoint = src->codepoint;
            dest->page_id = src->page_id;
        }
    }

    // Kernings
    if (asset->kernings.base.length)
    {
        out_bitmap_font->kernings = array_font_kerning_create(asset->glyphs.base.length);
        for (u32 i = 0; i < asset->kernings.base.length; ++i)
        {
            basset_bitmap_font_kerning* src = &asset->kernings.data[i];
            font_kerning* dest = &out_bitmap_font->kernings.data[i];

            dest->amount = src->amount;
            dest->codepoint_0 = src->codepoint_0;
            dest->codepoint_1 = src->codepoint_1;
        }
    }

    // Pages
    if (asset->pages.base.length)
    {
        out_bitmap_font->pages = array_font_page_create(asset->pages.base.length);
        for (u32 i = 0; i < asset->pages.base.length; ++i)
        {
            basset_bitmap_font_page* src = &asset->pages.data[i];
            font_page* dest = &out_bitmap_font->pages.data[i];

            dest->image_asset_name = src->image_asset_name;
        }
    }
    
    out_bitmap_font->base.state = BRESOURCE_STATE_LOADED;
}
