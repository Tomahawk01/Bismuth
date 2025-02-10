#pragma once

#include <assets/basset_types.h>
#include <defines.h>
#include <math/math_types.h>

typedef struct fnt_source_asset
{
    char* face_name;
    u32 size;
    b8 bold;
    b8 italic;
    b8 uniode;
    u32 line_height;
    u32 baseline;
    u32 atlas_size_x;
    u32 atlas_size_y;

    u32 glyph_count;
    basset_bitmap_font_glyph* glyphs;

    u32 kerning_count;
    basset_bitmap_font_kerning* kernings;
    
    u32 page_count;
    basset_bitmap_font_page* pages;
} fnt_source_asset;

BAPI b8 fnt_serializer_serialize(const fnt_source_asset* source_asset, const char** out_file_text);
BAPI b8 fnt_serializer_deserialize(const char* fnt_file_text, fnt_source_asset* out_fnt_source_asset);
