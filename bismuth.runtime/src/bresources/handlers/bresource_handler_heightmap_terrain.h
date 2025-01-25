#pragma once

#include "bresources/bresource_types.h"

struct bresource_handler;
struct bresource_request_info;

BAPI b8 bresource_handler_heightmap_terrain_request(struct bresource_handler* self, bresource* resource, const struct bresource_request_info* info);
BAPI void bresource_handler_heightmap_terrain_release(struct bresource_handler* self, bresource* resource);
