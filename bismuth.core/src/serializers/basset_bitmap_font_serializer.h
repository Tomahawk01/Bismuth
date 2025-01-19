#pragma once

#include "assets/basset_types.h"

BAPI void* basset_bitmap_font_serialize(const basset* asset, u64* out_size);
BAPI b8 basset_bitmap_font_deserialize(u64 size, const void* data, basset* out_asset);
