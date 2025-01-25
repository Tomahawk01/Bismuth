#pragma once

#include "bresources/bresource_types.h"

struct bresource_handler;
struct bresource_request_info;

BAPI b8 bresource_handler_text_request(struct bresource_handler* self, bresource* resource, const struct bresource_request_info* info);
BAPI void bresource_handler_text_release(struct bresource_handler* self, bresource* resource);
BAPI b8 bresource_handler_text_handle_hot_reload(struct bresource_handler* self, bresource* resource, basset* asset, u32 file_watch_id);
