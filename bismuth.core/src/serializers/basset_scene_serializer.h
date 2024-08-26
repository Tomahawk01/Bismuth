#pragma once

#include "assets/basset_types.h"

BAPI const char* basset_scene_serialize(const basset* asset);
BAPI b8 basset_scene_deserialize(const char* file_text, basset* out_asset);
