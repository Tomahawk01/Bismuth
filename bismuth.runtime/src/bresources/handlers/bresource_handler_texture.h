#pragma once

#include "bresources/bresource_types.h"

struct bresource_handler;

BAPI b8 bresource_handler_texture_request(struct bresource_handler* self, bresource* resource, bresource_request_info info);
BAPI void bresource_handler_texture_release(struct bresource_handler* self, bresource* resource);
