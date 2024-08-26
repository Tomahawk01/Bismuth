#pragma once

#include "assets/basset_types.h"

BAPI const char* basset_material_serialize(const basset* asset);
BAPI b8 basset_material_deserialize(const char* file_text, basset* out_asset);
