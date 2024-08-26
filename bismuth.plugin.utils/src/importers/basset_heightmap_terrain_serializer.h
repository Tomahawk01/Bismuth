#pragma once

#include "assets/basset_types.h"

BAPI const char* basset_heightmap_terrain_serialize(const basset* asset);
BAPI b8 basset_heightmap_terrain_deserialize(const char* file_text, basset* out_asset);
