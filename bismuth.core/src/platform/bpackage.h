#pragma once

#include "defines.h"
#include "platform/vfs.h"
#include "strings/bname.h"

typedef struct asset_manifest_asset
{
    bname name;
    const char* path;
    const char* source_path;
} asset_manifest_asset;

/** @brief A reference to another package in an asset manifest */
typedef struct asset_manifest_reference
{
    bname name;
    const char* path;
} asset_manifest_reference;

typedef struct asset_manifest
{
    bname name;
    // Path to .bpackage file. Null if loading from disk
     const char* file_path;
    // Path containing the .bpackage file, without the filename itself
    const char* path;

    // darray
    asset_manifest_asset* assets;

    // darray
    asset_manifest_reference* references;
} asset_manifest;

struct bpackage_internal;

typedef struct bpackage
{
    bname name;
    b8 is_binary;
    struct bpackage_internal* internal_data;
    // darray of file ids that are being watched
    u32* watch_ids;
} bpackage;

typedef enum bpackage_result
{
    BPACKAGE_RESULT_SUCCESS = 0,
    BPACKAGE_RESULT_PRIMARY_GET_FAILURE,
    BPACKAGE_RESULT_SOURCE_GET_FAILURE,
    BPACKAGE_RESULT_INTERNAL_FAILURE
} bpackage_result;

BAPI b8 bpackage_create_from_manifest(const asset_manifest* manifest, bpackage* out_package);
BAPI b8 bpackage_create_from_binary(u64 size, void* bytes, bpackage* out_package);
BAPI void bpackage_destroy(bpackage* package);

BAPI bpackage_result bpackage_asset_bytes_get(const bpackage* package, bname name, b8 get_source, u64* out_size, const void** out_data);
BAPI bpackage_result bpackage_asset_text_get(const bpackage* package, bname name, b8 get_source, u64* out_size, const char** out_text);
BAPI b8 bpackage_asset_watch(bpackage* package, const char* asset_path, u32* out_watch_id);
BAPI void bpackage_asset_unwatch(bpackage* package, u32 watch_id);

BAPI const char* bpackage_path_for_asset(const bpackage* package, bname name);
BAPI const char* bpackage_source_path_for_asset(const bpackage* package, bname name);

BAPI b8 bpackage_asset_bytes_write(bpackage* package, bname name, u64 size, const void* bytes);
BAPI b8 bpackage_asset_text_write(bpackage* package, bname name, u64 size, const char* text);

BAPI b8 bpackage_parse_manifest_file_content(const char* path, asset_manifest* out_manifest);
BAPI void bpackage_manifest_destroy(asset_manifest* manifest);
