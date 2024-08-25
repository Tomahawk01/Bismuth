#pragma once

#include "defines.h"

typedef struct asset_manifest_asset
{
    const char* name;
    const char* type;
    const char* path;
} asset_manifest_asset;

/** @brief A reference to another package in an asset manifest */
typedef struct asset_manifest_reference
{
    const char* name;
    const char* path;
} asset_manifest_reference;

typedef struct asset_manifest
{
    const char* name;
    // Path to .bpackage file. Null if loading from disk
    const char* path;

    u32 asset_count;
    asset_manifest_asset* assets;

    u32 reference_count;
    asset_manifest_reference* references;
} asset_manifest;

struct bpackage_internal;

typedef struct bpackage
{
    const char* name;
    b8 is_binary;
    struct bpackage_internal* internal_data;
} bpackage;

BAPI b8 bpackage_create_from_manifest(const asset_manifest* manifest, bpackage* out_package);
BAPI b8 bpackage_create_from_binary(u64 size, void* bytes, bpackage* out_package);
BAPI void bpackage_destroy(bpackage* package);

BAPI void* bpackage_asset_bytes_get(const bpackage* package, const char* type, const char* name, u64* out_size);
