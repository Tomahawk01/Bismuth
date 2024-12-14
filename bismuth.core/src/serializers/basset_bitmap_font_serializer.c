#include "basset_bitmap_font_serializer.h"

#include "assets/basset_types.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "strings/bname.h"
#include "strings/bstring.h"

#include <stdio.h>

typedef struct bitmap_font_header
{
    // The base binary asset header. Must always be the first member
    binary_asset_header base;

    u32 font_size;
    i32 line_height;
    i32 baseline;
    i32 atlas_size_x;
    i32 atlas_size_y;
    u32 glyph_count;
    u32 kerning_count;
    u32 page_count;
    u32 face_name_len;
} bitmap_font_header;

void* basset_bitmap_font_serialize(const basset* asset, u64* out_size)
{
    if (!asset)
    {
        BERROR("Cannot serialize without an asset");
        return 0;
    }

    if (asset->type != BASSET_TYPE_BITMAP_FONT)
    {
        BERROR("Cannot serialize a non-bitmap_font asset using the bitmap_font serializer");
        return 0;
    }

    // File layout is header, face name string, glyphs, kernings, pages
    bitmap_font_header header = {0};

    // Base attributes
    header.base.magic = ASSET_MAGIC;
    header.base.type = (u32)asset->type;
    header.base.data_block_size = 0;
    // Always write the most current version
    header.base.version = 1;

    basset_bitmap_font* typed_asset = (basset_bitmap_font*)asset;

    const char* face_str = bname_string_get(typed_asset->face);
    header.face_name_len = string_length(face_str);

    header.font_size = typed_asset->size;
    header.line_height = typed_asset->line_height;
    header.baseline = typed_asset->baseline;
    header.atlas_size_x = typed_asset->atlas_size_x;
    header.atlas_size_y = typed_asset->atlas_size_y;
    header.glyph_count = typed_asset->glyphs.base.length;
    header.kerning_count = typed_asset->kernings.base.length;
    header.page_count = typed_asset->pages.base.length;

    // Calculate the total required size first (for everything after the header
    header.base.data_block_size += header.face_name_len;
    header.base.data_block_size += (typed_asset->glyphs.base.stride * typed_asset->glyphs.base.length);
    header.base.data_block_size += (typed_asset->kernings.base.stride * typed_asset->kernings.base.length);

    // Iterate pages and save the length, then the string asset name for each
    for (u32 i = 0; i < typed_asset->pages.base.length; ++i)
    {
        const char* str = bname_string_get(typed_asset->pages.data[i].image_asset_name);
        u32 len = string_length(str);
        header.base.data_block_size += sizeof(u32); // For the length
        header.base.data_block_size += len;         // For the actual string
    }

    // The total space required for the data block
    *out_size = sizeof(bitmap_font_header) + header.base.data_block_size;

    // Allocate said block
    void* block = ballocate(*out_size, MEMORY_TAG_SERIALIZER);
    // Write the header
    bcopy_memory(block, &header, sizeof(bitmap_font_header));

    // For this asset, it's not quite a simple manner of just using the byte block.
    // Start by moving past the header
    u64 offset = sizeof(bitmap_font_header);

    // Face name
    bcopy_memory(block + offset, face_str, header.face_name_len);
    offset += header.face_name_len;

    // Glyphs can be written as-is
    u64 glyph_size = (typed_asset->glyphs.base.stride * typed_asset->glyphs.base.length);
    bcopy_memory(block + offset, typed_asset->glyphs.data, glyph_size);
    offset += glyph_size;

    // Kernings can be written as-is
    u64 kerning_size = (typed_asset->kernings.base.stride * typed_asset->kernings.base.length);
    bcopy_memory(block + offset, typed_asset->kernings.data, kerning_size);
    offset += kerning_size;

    // Pages need to write asset name string length, then the actual string
    for (u32 i = 0; i < typed_asset->pages.base.length; ++i)
    {
        const char* str = bname_string_get(typed_asset->pages.data[i].image_asset_name);
        u32 len = string_length(str);

        bcopy_memory(block + offset, &len, sizeof(u32));
        offset += sizeof(u32);

        bcopy_memory(block + offset, str, sizeof(char) * len);
        offset += len;
    }

    // Return the serialized block of memory
    return block;
}

b8 basset_bitmap_font_deserialize(const void* data, basset* out_asset)
{
    //
}
