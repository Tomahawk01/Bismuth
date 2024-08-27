#include "asset_handler_bitmap_font.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <platform/vfs.h>
#include <serializers/basset_bitmap_font_serializer.h>
#include <strings/bstring.h>

void asset_handler_bitmap_font_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = false;
    self->request_asset = 0;
    self->release_asset = asset_handler_bitmap_font_release_asset;
    self->type = BASSET_TYPE_BITMAP_FONT;
    self->type_name = BASSET_TYPE_NAME_BITMAP_FONT;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = 0; // NOTE: Intentionally not set as serializing to this format makes no sense
    self->text_deserialize = basset_bitmap_font_deserialize;
}

void asset_handler_bitmap_font_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_bitmap_font* typed_asset = (basset_bitmap_font*)asset;

    /* if (typed_asset->pages && typed_asset->page_count)
    {
        bfree(typed_asset->pages, sizeof(basset_bitmap_font_page) * typed_asset->page_count, MEMORY_TAG_ARRAY);
        typed_asset->pages = 0;
        typed_asset->page_count = 0;
    }
    if (typed_asset->glyphs && typed_asset->glyph_count)
    {
        bfree(typed_asset->glyphs, sizeof(basset_bitmap_font_glyph) * typed_asset->glyph_count, MEMORY_TAG_ARRAY);
        typed_asset->glyphs = 0;
        typed_asset->glyph_count = 0;
    }
    if (typed_asset->kernings && typed_asset->kerning_count)
    {
        bfree(typed_asset->kernings, sizeof(basset_bitmap_font_kerning) * typed_asset->kerning_count, MEMORY_TAG_ARRAY);
        typed_asset->kernings = 0;
        typed_asset->kerning_count = 0;
    } */

    array_basset_bitmap_font_page_destroy(&typed_asset->pages);
    array_basset_bitmap_font_glyph_destroy(&typed_asset->glyphs);
    array_basset_bitmap_font_kerning_destroy(&typed_asset->kernings);

    bzero_memory(typed_asset, sizeof(basset_bitmap_font));
}
