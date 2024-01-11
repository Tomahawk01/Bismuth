#pragma once

#include "systems/resource_system.h"

resource_loader image_resource_loader_create(void);

BAPI b8 image_loader_query_properties(const char *image_name, i32 *out_width, i32 *out_height, i32 *out_channels, u32 *out_mip_levels);
