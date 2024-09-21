#pragma once

#include "defines.h"
#include "strings/bname.h"

struct bpackage;
struct basset;
struct basset_metadata;

typedef struct vfs_state
{
    // darray
    struct bpackage* packages;
} vfs_state;

typedef struct vfs_config
{
    const char** text_user_types;
} vfs_config;

typedef enum vfs_asset_flag_bits
{
    VFS_ASSET_FLAG_NONE = 0,
    VFS_ASSET_FLAG_BINARY_BIT = 0x01,
    VFS_ASSET_FLAG_FROM_SOURCE = 0x02
} vfs_asset_flag_bits;

typedef u32 vfs_asset_flags;

typedef enum vfs_request_result
{
    /** The request was fulfilled successfully */
    VFS_REQUEST_RESULT_SUCCESS = 0,
    /** The asset exists on the manifest, but the primary file could not be found on disk */
    VFS_REQUEST_RESULT_FILE_DOES_NOT_EXIST,
    /** The asset exists on the manifest, but the source file could not be found on disk */
    VFS_REQUEST_RESULT_SOURCE_FILE_DOES_NOT_EXIST,
    /** The package does not contain the asset */
    VFS_REQUEST_RESULT_NOT_IN_PACKAGE,
    /** Package does not exist */
    VFS_REQUEST_RESULT_PACKAGE_DOES_NOT_EXIST,
    /** There was an error reading from the file */
    VFS_REQUEST_RESULT_READ_ERROR,
    /** There was an error writing to the file */
    VFS_REQUEST_RESULT_WRITE_ERROR,
    /** An internal failure has occurred in the VFS itself */
    VFS_REQUEST_RESULT_INTERNAL_FAILURE,
} vfs_request_result;

typedef struct vfs_asset_data
{
    /** @brief The name of the asset stored as a bame */
    bname asset_name;
    /** @brief The name of the package containing the asset, stored as a bname */
    bname package_name;
    /** @brief A copy of the asset/source asset path */
    const char* path;

    u64 size;
    union
    {
        const char* text;
        const void* bytes;
    };
    vfs_asset_flags flags;

    vfs_request_result result;

    u32 context_size;
    void* context;

    u32 import_params_size;
    void* import_params;
} vfs_asset_data;

typedef void (*PFN_on_asset_loaded_callback)(struct vfs_state* vfs, vfs_asset_data asset_data);

BAPI b8 vfs_initialize(u64* memory_requirement, vfs_state* out_state, const vfs_config* config);
BAPI void vfs_shutdown(vfs_state* state);

BAPI void vfs_request_asset(vfs_state* state, bname package_name, bname asset_name, b8 is_binary, b8 get_source, u32 context_size, const void* context, u32 import_params_size, void* import_params, PFN_on_asset_loaded_callback callback);
BAPI void vfs_request_asset_sync(vfs_state* state, bname package_name, bname asset_name, b8 is_binary, b8 get_source, u32 context_size, const void* context, u32 import_params_size, void* import_params, vfs_asset_data* out_data);

BAPI const char* vfs_path_for_asset(vfs_state* state, bname package_name, bname asset_name);
BAPI const char* vfs_source_path_for_asset(vfs_state* state, bname package_name, bname asset_name);

BAPI void vfs_request_direct_from_disk(vfs_state* state, const char* path, b8 is_binary, u32 context_size, const void* context, PFN_on_asset_loaded_callback callback);
BAPI void vfs_request_direct_from_disk_sync(vfs_state* state, const char* path, b8 is_binary, u32 context_size, const void* context, vfs_asset_data* out_data);

BAPI b8 vfs_asset_write(vfs_state* state, const struct basset* asset, b8 is_binary, u64 size, const void* data);
