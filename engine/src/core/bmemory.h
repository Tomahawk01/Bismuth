#pragma once

#include "defines.h"

typedef enum memory_tag
{
    // For temporary use. Should be assigned one of the below or have a new tag created
    MEMORY_TAG_UNKNOWN,
    MEMORY_TAG_ARRAY,
    MEMORY_TAG_LINEAR_ALLOCATOR,
    MEMORY_TAG_DARRAY,
    MEMORY_TAG_DICT,
    MEMORY_TAG_RING_QUEUE,
    MEMORY_TAG_BST,
    MEMORY_TAG_STRING,
    MEMORY_TAG_ENGINE,
    MEMORY_TAG_JOB,
    MEMORY_TAG_TEXTURE,
    MEMORY_TAG_MATERIAL_INSTANCE,
    MEMORY_TAG_RENDERER,
    MEMORY_TAG_GAME,
    MEMORY_TAG_TRANSFORM,
    MEMORY_TAG_ENTITY,
    MEMORY_TAG_ENTITY_NODE,
    MEMORY_TAG_SCENE,
    MEMORY_TAG_RESOURCE,
    MEMORY_TAG_VULKAN,
    // "External" vulkan allocations, for reporting purposes only
    MEMORY_TAG_VULKAN_EXT,
    MEMORY_TAG_DIRECT3D,
    MEMORY_TAG_OPENGL,
    // Representation of GPU-local/vram
    MEMORY_TAG_GPU_LOCAL,
    MEMORY_TAG_BITMAP_FONT,
    MEMORY_TAG_SYSTEM_FONT,
    MEMORY_TAG_KEYMAP,
    MEMORY_TAG_HASHTABLE,

    MEMORY_TAG_MAX_TAGS
} memory_tag;

/** @brief Configuration for the memory system. */
typedef struct memory_system_configuration
{
    // Total memory size in bytes used by the internal allocator for this system
    u64 total_alloc_size;
} memory_system_configuration;

b8 memory_system_initialize(memory_system_configuration config);
void memory_system_shutdown(void* state);

BAPI void* ballocate(u64 size, memory_tag tag);

BAPI void* ballocate_aligned(u64 size, u16 alignment, memory_tag tag);

BAPI void ballocate_report(u64 size, memory_tag tag);

BAPI void bfree(void* block, u64 size, memory_tag tag);

BAPI void bfree_aligned(void* block, u64 size, u16 alignment, memory_tag tag);

BAPI void bfree_report(u64 size, memory_tag tag);

BAPI b8 bmemory_get_size_alignment(void* block, u64* out_size, u16* out_alignment);

BAPI void* bzero_memory(void* block, u64 size);

BAPI void* bcopy_memory(void* dest, const void* source, u64 size);

BAPI void* bset_memory(void* dest, i32 value, u64 size);

BAPI char* get_memory_usage_str(void);

BAPI u64 get_memory_alloc_count(void);
