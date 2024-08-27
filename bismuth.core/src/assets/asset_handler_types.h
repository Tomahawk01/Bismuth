#pragma once

#include "assets/basset_types.h"

struct basset;
struct vfs_state;

typedef struct asset_handler
{
    basset_type type;
    const char* type_name;
    b8 is_binary;

    /** @brief Cache a pointer to the VFS state for fast lookup */
    struct vfs_state* vfs;

    /** @brief Requests an asset from the given handler */
    void (*request_asset)(struct asset_handler* self, struct basset* asset, void* listener_instance, PFN_basset_on_result user_callback);
    void (*release_asset)(struct asset_handler* self, struct basset* asset);

    void* (*binary_serialize)(const basset* asset, u64* out_size);
    b8 (*binary_deserialize)(u64 size, const void* block, basset* out_asset);

    const char* (*text_serialize)(const basset* asset);
    b8 (*text_deserialize)(const char* file_text, basset* out_asset);
} asset_handler;

typedef struct asset_handler_request_context
{
    struct asset_handler* handler;
    void* listener_instance;
    PFN_basset_on_result user_callback;
    struct basset* asset;
} asset_handler_request_context;
