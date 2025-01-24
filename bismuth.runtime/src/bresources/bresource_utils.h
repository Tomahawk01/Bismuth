#pragma once
#include <assets/basset_types.h>

#include "bresource_types.h"

BAPI texture_format image_format_to_texture_format(basset_image_format format);
BAPI basset_image_format texture_format_to_image_format(texture_format format);
BAPI u8 channel_count_from_texture_format(texture_format format);

BAPI const char* bresource_type_to_string(bresource_type type);
