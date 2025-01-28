#pragma once

#include "assets/basset_types.h"
#include "defines.h"
#include "strings/bname.h"

struct bpackage;
struct basset;
struct basset_metadata;

struct vfs_state;

typedef struct vfs_config
{
    const char** text_user_types;
    const char* manifest_file_path;
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
    /** @brief A copy of the source asset path (if the asset itself is not a source asset, otherwise 0) */
    const char* source_asset_path;

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

    b8 watch_for_hot_reload;

    // The file watch id if used during a hot-reload. otherwise INVALID_ID
    u32 file_watch_id;
} vfs_asset_data;

typedef void (*PFN_on_asset_loaded_callback)(struct vfs_state* vfs, vfs_asset_data asset_data);

typedef void (*PFN_asset_hot_reloaded_callback)(void* listener, const vfs_asset_data* asset_data);
typedef void (*PFN_asset_deleted_callback)(void* listener, u32 file_watch_id);

typedef struct vfs_state
{
    // darray
    struct bpackage* packages;
    // darray
    vfs_asset_data* watched_assets;
    // A pointer to a state listening for asset hot reloads
    void* hot_reload_listener;
    // A callback to be made when an asset is hot-reloaded from the VFS. Typically handled within the asset system
    PFN_asset_hot_reloaded_callback hot_reloaded_callback;
    // A pointer to a state listening for asset deletions from disk
    void* deleted_listener;
    // A callback to be made when an asset is deleted from the VFS. Typically handled within the asset system
    PFN_asset_deleted_callback deleted_callback;
} vfs_state;

/** @brief The request options for getting an asset from the VFS */
typedef struct vfs_request_info
{
    /** @brief The name of the package to load the asset from */
    bname package_name;
    /** @brief The name of the asset to request */
    bname asset_name;
    /** @brief Indicates if the asset is binary. If not, the asset is loaded as text */
    b8 is_binary;
    /** @brief Indicates if the VFS should try to retrieve the source asset instead of the primary one if it exists */
    b8 get_source;
    /** @brief Indicates if the asset's file on-disk should be watched for hot-reload */
    b8 watch_for_hot_reload;
    /** @brief The size of the context in bytes */
    u32 context_size;
    /** @param context A constant pointer to the context to be used for this call. This is passed through to the result callback. NOTE: A copy of this is taken immediately, so lifetime of this isn't important */
    const void* context;
    /** @brief The size of the import parameters in bytes, if used */
    u32 import_params_size;
    /** @param context A constant pointer to the import parameters to be used for this call. NOTE: A copy of this is taken immediately, so lifetime of this isn't important */
    void* import_params;
    PFN_on_asset_loaded_callback vfs_callback;
} vfs_request_info;

BAPI b8 vfs_initialize(u64* memory_requirement, vfs_state* out_state, const vfs_config* config);
BAPI void vfs_shutdown(vfs_state* state);

BAPI void vfs_hot_reload_callbacks_register(vfs_state* state, void* hot_reload_listener, PFN_asset_hot_reloaded_callback hot_reloaded_callback, void* deleted_listener, PFN_asset_deleted_callback deleted_callback);

BAPI void vfs_request_asset(vfs_state* state, vfs_request_info info);
BAPI vfs_asset_data vfs_request_asset_sync(vfs_state* state, vfs_request_info info);

BAPI const char* vfs_path_for_asset(vfs_state* state, bname package_name, bname asset_name);
BAPI const char* vfs_source_path_for_asset(vfs_state* state, bname package_name, bname asset_name);

BAPI void vfs_request_direct_from_disk(vfs_state* state, const char* path, b8 is_binary, u32 context_size, const void* context, PFN_on_asset_loaded_callback callback);
BAPI void vfs_request_direct_from_disk_sync(vfs_state* state, const char* path, b8 is_binary, u32 context_size, const void* context, vfs_asset_data* out_data);

BAPI b8 vfs_asset_write(vfs_state* state, const struct basset* asset, b8 is_binary, u64 size, const void* data);
