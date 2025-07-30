#pragma once

#include "assets/basset_types.h"

BAPI void* basset_image_serialize(const basset_image* asset, u64* out_size);
BAPI b8 basset_image_deserialize(u64 size, const void* block, basset_image* out_asset);
