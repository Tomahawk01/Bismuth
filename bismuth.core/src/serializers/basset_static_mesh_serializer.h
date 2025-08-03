#pragma once

#include "assets/basset_types.h"

BAPI void* basset_static_mesh_serialize(const basset_static_mesh* asset, u64* out_size);
BAPI b8 basset_static_mesh_deserialize(u64 size, const void* block, basset_static_mesh* out_asset);
