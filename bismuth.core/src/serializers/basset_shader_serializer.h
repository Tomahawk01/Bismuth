#pragma once

#include "assets/basset_types.h"

BAPI const char* basset_shader_serialize(const basset* asset);
BAPI b8 basset_shader_deserialize(const char* file_text, basset* out_asset);
