#include "basset_importer_bitmap_font_fnt.h"

#include <assets/basset_types.h>
#include <core/engine.h>
#include <core_render_types.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <serializers/basset_binary_static_mesh_serializer.h>
#include <serializers/basset_material_serializer.h>
#include <strings/bname.h>
#include <strings/bstring.h>

#include "resources/font_types.h"
#include "serializers/fnt_serializer.h"

b8 basset_importer_bitmap_font_fnt(const struct basset_importer* self, u64 data_size, const void* data, void* params, struct basset* out_asset)
{
    if (!self || !data_size || !data)
    {
        BERROR("basset_importer_bitmap_font_fnt requires valid pointers to self and data, as well as a nonzero data_size");
        return false;
    }
    basset_bitmap_font* typed_asset = (basset_bitmap_font*)out_asset;

    struct vfs_state* vfs = engine_systems_get()->vfs_system_state;

    // Handle OBJ file import
    {
        fnt_source_asset fnt_asset = {0};
        if (!fnt_serializer_deserialize(data, &fnt_asset))
        {
            BERROR("FNT file import failed! See logs for details");
            return false;
        }

        // Convert OBJ asset to static_mesh
        typed_asset->base.type = KASSET_TYPE_BITMAP_FONT;
        typed_asset->base.name = bname_create(fnt_asset.face_name);
        typed_asset->baseline = fnt_asset.baseline;
        typed_asset->face = bname_create(fnt_asset.face_name);
        typed_asset->size = fnt_asset.size;
        typed_asset->line_height = fnt_asset.line_height;
        typed_asset->atlas_size_x = fnt_asset.atlas_size_x;
        typed_asset->atlas_size_y = fnt_asset.atlas_size_y;

        typed_asset->pages = array_basset_bitmap_font_page_create(fnt_asset.page_count);
        BCOPY_TYPE_CARRAY(typed_asset->pages.data, fnt_asset.pages, bitmap_font_page, fnt_asset.page_count);

        typed_asset->glyphs = array_basset_bitmap_font_glyph_create(fnt_asset.glyph_count);
        BCOPY_TYPE_CARRAY(typed_asset->glyphs.data, fnt_asset.glyphs, font_glyph, fnt_asset.glyph_count);

        if (fnt_asset.kerning_count)
        {
            typed_asset->kernings = array_basset_bitmap_font_kerning_create(fnt_asset.kerning_count);
            BCOPY_TYPE_CARRAY(typed_asset->kernings.data, fnt_asset.kernings, font_kerning, fnt_asset.kerning_count);
        }

        // Cleanup fnt asset
        BFREE_TYPE_CARRAY(fnt_asset.pages, bitmap_font_page, fnt_asset.page_count);
        BFREE_TYPE_CARRAY(fnt_asset.glyphs, font_glyph, fnt_asset.glyph_count);
        BFREE_TYPE_CARRAY(fnt_asset.kernings, font_kerning, fnt_asset.kerning_count);
        string_free(fnt_asset.face_name);
    }

    // Serialize data and write out kbf file (binary Bismuth Bitmap Font)
    {
        u64 serialized_size = 0;
        void* serialized_data = basset_binary_static_mesh_serialize(out_asset, &serialized_size);
        if (!serialized_data || !serialized_size)
        {
            BERROR("Failed to serialize binary Bismuth Bitmap Font");
            return false;
        }

        // Write out .bbf file
        if (!vfs_asset_write(0, out_asset, true, serialized_size, serialized_data))
        {
            BWARN("Failed to write .kbf (Bismuth Bitmap Font) file. See logs for details");
        }
    }

    return true;
}
