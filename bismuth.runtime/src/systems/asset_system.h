#pragma once

#include <assets/basset_types.h>
#include <strings/bname.h>

typedef struct asset_system_config
{
    // The maximum number of assets which may be loaded at once
    u32 max_asset_count;
} asset_system_config;

struct asset_system_state;

typedef struct asset_request_info
{
    /** @param type The asset type */
    basset_type type;
    /** @param package_name The name of the package */
    bname package_name;
    /** @param asset_name The name of the asset */
    bname asset_name;
    // If true, request is synchronous and does not return until asset is read and processed
    b8 synchronous;
    /** @param auto_release Indicates if the asset should be released automatically when its internal reference counter reaches 0. Only has an effect the first time the asset is requested */
    b8 auto_release;
    /** @param listener_inst A pointer to the listener instance that is awaiting the asset. Technically optional as perhaps nothing is interested in the result, but why? */
    void* listener_inst;
    /** @param callback A pointer to the function to be called when the load is complete (or failed). Technically optional as perhaps nothing is interested in the result, but why? */
    PFN_basset_on_result callback;
    /** @param import_params_size Size of the import params, if used; otherwise 0 */
    u32 import_params_size;
    /** @param import_params A pointer to the inport params, if used; otherwise 0 */
    void* import_params;
    /** @param hot_reload_callback A callback to be made if the asset is hot-reloaded. Pass 0 if not used. Hot-reloaded assets are not auto-released */
    PFN_basset_on_hot_reload hot_reload_callback;
    /** @param hot_reload_listener A pointer to the listener data for an asset hot-reload */
    void* hot_reload_context;
} asset_request_info;

BAPI b8 asset_system_deserialize_config(const char* config_str, asset_system_config* out_config);

BAPI b8 asset_system_initialize(u64* memory_requirement, struct asset_system_state* state, const asset_system_config* config);
BAPI void asset_system_shutdown(struct asset_system_state* state);

BAPI void asset_system_request(struct asset_system_state* state, asset_request_info info);
BAPI void asset_system_release(struct asset_system_state* state, bname asset_name, bname package_name);

BAPI void asset_system_on_handler_result(struct asset_system_state* state, asset_request_result result, basset* asset, void* listener_instance, PFN_basset_on_result callback);

BAPI b8 asset_type_is_binary(basset_type type);
