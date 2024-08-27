#pragma once

#include "strings/bname.h"
#include <assets/basset_types.h>

typedef struct asset_system_config
{
    // The maximum number of assets which may be loaded at once
    u32 max_asset_count;
} asset_system_config;

struct asset_system_state;

BAPI b8 asset_system_deserialize_config(const char* config_str, asset_system_config* out_config);

BAPI b8 asset_system_initialize(u64* memory_requirement, struct asset_system_state* state, const asset_system_config* config);
BAPI void asset_system_shutdown(struct asset_system_state* state);

BAPI void asset_system_request(struct asset_system_state* state, basset_type type, bname package_name, bname asset_name, b8 auto_release, void* listener_inst, PFN_basset_on_result callback);
BAPI void asset_system_release(struct asset_system_state* state, bname package_name, bname asset_name);

BAPI void asset_system_on_handler_result(struct asset_system_state* state, asset_request_result result, basset* asset, void* listener_instance, PFN_basset_on_result callback);

BAPI b8 asset_type_is_binary(basset_type type);
