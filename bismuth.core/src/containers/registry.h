#pragma once

#include "defines.h"
#include "identifiers/bhandle.h"

typedef enum bregistry_entry_change_type
{
    // The block of memory was changed/replaced
    B_REGISTRY_CHANGE_TYPE_BLOCK_CHANGED,
    // The block of memory/the entry was destroyed
    B_REGISTRY_CHANGE_TYPE_DESTROYED
} bregistry_entry_change_type;

// Callback to be made when a registry block is updated via registry_entry_block_set()
typedef void (*PFN_on_registry_entry_updated)(void* sender, void* block, u64 size, bregistry_entry_change_type change_type);

typedef struct bregistry_entry_listener_callback
{
    void* listener;
    PFN_on_registry_entry_updated callback;
} bregistry_entry_listener_callback;

typedef struct bregistry_entry
{
    u64 uniqueid;
    u64 block_size;
    void* block;
    i32 reference_count;
    b8 auto_release;

    // darray
    bregistry_entry_listener_callback* callbacks;
} bregistry_entry;

typedef struct b_registry
{
    // darray
    bregistry_entry* entries;
} bregistry;

BAPI void bregistry_create(bregistry* out_registry);
BAPI void bregistry_destroy(bregistry* registry);

BAPI b_handle bregistry_add_entry(bregistry* registry, const void* block, u64 size, b8 auto_release);
BAPI b8 bregistry_entry_set(bregistry* registry, b_handle entry_handle, const void* block, u64 size, void* sender);
BAPI b8 bregistry_entry_update_callback_for_listener(bregistry* registry, b_handle entry_handle, void* listener, PFN_on_registry_entry_updated updated_callback);
BAPI void* bregistry_entry_acquire(bregistry* registry, b_handle entry_handle, void* listener, PFN_on_registry_entry_updated updated_callback);
BAPI void bregistry_entry_release(bregistry* registry, b_handle entry_handle, void* listener);
