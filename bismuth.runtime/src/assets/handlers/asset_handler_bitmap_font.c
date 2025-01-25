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
    self->is_binary = true;
    self->request_asset = 0;
    self->release_asset = asset_handler_bitmap_font_release_asset;
    self->type = BASSET_TYPE_BITMAP_FONT;
    self->type_name = BASSET_TYPE_NAME_BITMAP_FONT;
    self->binary_serialize = basset_bitmap_font_serialize;
    self->binary_deserialize = basset_bitmap_font_deserialize;
    self->text_serialize = 0;
    self->text_deserialize = 0;
    self->size = sizeof(basset_bitmap_font);
}

void asset_handler_bitmap_font_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_bitmap_font* typed_asset = (basset_bitmap_font*)asset;

    array_basset_bitmap_font_page_destroy(&typed_asset->pages);
    array_basset_bitmap_font_glyph_destroy(&typed_asset->glyphs);
    array_basset_bitmap_font_kerning_destroy(&typed_asset->kernings);

    bzero_memory(typed_asset, sizeof(basset_bitmap_font));
}
