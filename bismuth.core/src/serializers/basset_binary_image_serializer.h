#pragma once

#include "assets/basset_types.h"

BAPI void* basset_binary_image_serialize(const basset* asset, u64* out_size);
BAPI b8 basset_binary_image_deserialize(u64 size, const void* block, basset* out_asset);
