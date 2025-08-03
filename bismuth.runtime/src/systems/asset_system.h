#pragma once

#include <assets/basset_types.h>
#include <strings/bname.h>

typedef struct asset_system_config
{
    // The maximum number of assets which may be loaded at once
    u32 max_asset_count;

    bname application_package_name;
    const char* application_package_name_str;
} asset_system_config;

struct asset_system_state;

BAPI b8 asset_system_deserialize_config(const char* config_str, asset_system_config* out_config);

BAPI b8 asset_system_initialize(u64* memory_requirement, struct asset_system_state* state, const asset_system_config* config);
BAPI void asset_system_shutdown(struct asset_system_state* state);

// -----------------------------------
// ========== BINARY ASSETS ==========
// -----------------------------------

typedef void (*PFN_basset_binary_loaded_callback)(void* listener, basset_binary* asset);
// async load from game package
BAPI basset_binary* asset_system_request_binary(struct asset_system_state* state, const char* name, void* listener, PFN_basset_binary_loaded_callback callback);
// sync load from game package
BAPI basset_binary* asset_system_request_binary_sync(struct asset_system_state* state, const char* name);
// async load from specific package
BAPI basset_binary* asset_system_request_binary_from_package(struct asset_system_state* state, const char* package_name, const char* name, void* listener, PFN_basset_binary_loaded_callback callback);
// sync load from specific package
BAPI basset_binary* asset_system_request_binary_from_package_sync(struct asset_system_state* state, const char* package_name, const char* name);

BAPI void asset_system_release_binary(struct asset_system_state* state, basset_binary* asset);

// ----------------------------------
// ========== IMAGE ASSETS ==========
// ----------------------------------

typedef void (*PFN_basset_image_loaded_callback)(void* listener, basset_image* asset);
// async load from game package
BAPI basset_image* asset_system_request_image(struct asset_system_state* state, const char* name, b8 flip_y, void* listener, PFN_basset_image_loaded_callback callback);
// sync load from game package
BAPI basset_image* asset_system_request_image_sync(struct asset_system_state* state, const char* name, b8 flip_y);
// async load from specific package
BAPI basset_image* asset_system_request_image_from_package(struct asset_system_state* state, const char* package_name, const char* name, b8 flip_y, void* listener, PFN_basset_image_loaded_callback callback);
// sync load from specific package
BAPI basset_image* asset_system_request_image_from_package_sync(struct asset_system_state* state, const char* package_name, const char* name, b8 flip_y);

BAPI void asset_system_release_image(struct asset_system_state* state, basset_image* asset);

// -----------------------------------
// ======== BITMAP FONT ASSETS =======
// -----------------------------------

// sync load from game package
BAPI basset_bitmap_font* asset_system_request_bitmap_font_sync(struct asset_system_state* state, const char* name);
// sync load from specific package
BAPI basset_bitmap_font* asset_system_request_bitmap_font_from_package_sync(struct asset_system_state* state, const char* package_name, const char* name);

BAPI void asset_system_release_bitmap_font(struct asset_system_state* state, basset_bitmap_font* asset);

// -----------------------------------
// ======== SYSTEM FONT ASSETS =======
// -----------------------------------

// sync load from game package
BAPI basset_system_font* asset_system_request_system_font_sync(struct asset_system_state* state, const char* name);

// sync load from specific package
BAPI basset_system_font* asset_system_request_system_font_from_package_sync(struct asset_system_state* state, const char* package_name, const char* name);

BAPI void asset_system_release_system_font(struct asset_system_state* state, basset_system_font* asset);
