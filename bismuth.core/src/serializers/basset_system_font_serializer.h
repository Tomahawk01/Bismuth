#pragma once

#include "assets/basset_types.h"

BAPI const char* basset_system_font_serialize(const basset* asset);
BAPI b8 basset_system_font_deserialize(const char* file_text, basset* out_asset);
