#pragma once

#include "bresources/bresource_types.h"

struct bresource_handler;
struct bresource_request_info;

BAPI bresource* bresource_handler_text_allocate(void);
BAPI b8 bresource_handler_text_request(struct bresource_handler* self, bresource* resource, const struct bresource_request_info* info);
BAPI void bresource_handler_text_release(struct bresource_handler* self, bresource* resource);
