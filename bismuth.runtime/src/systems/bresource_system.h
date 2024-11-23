#pragma once

#include "bresources/bresource_types.h"

struct bresource_system_state;

typedef struct bresource_system_config
{
    u32 max_resource_count;
} bresource_system_config;

typedef struct bresource_handler
{
    struct asset_system_state* asset_system;
    bresource* (*allocate)(void);
    b8 (*request)(struct bresource_handler* self, bresource* resource, const struct bresource_request_info* info);
    void (*release)(struct bresource_handler* self, bresource* resource);
} bresource_handler;

BAPI b8 bresource_system_initialize(u64* memory_requirement, struct bresource_system_state* state, const bresource_system_config* config);
BAPI void bresource_system_shutdown(struct bresource_system_state* state);

BAPI bresource* bresource_system_request(struct bresource_system_state* state, bname name, const struct bresource_request_info* info);
BAPI void bresource_system_release(struct bresource_system_state* state, bname resource_name);

BAPI b8 bresource_system_handler_register(struct bresource_system_state* state, bresource_type type, bresource_handler handler);
