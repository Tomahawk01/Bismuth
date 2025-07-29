#pragma once

#include "defines.h"

// Interface for a frame allocator
typedef struct frame_allocator_int
{
    void* (*allocate)(u64 size);
    void (*free)(void* block, u64 size);
    void (*free_all)(void);
} frame_allocator_int;

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
    MEMORY_TAG_UI,
    MEMORY_TAG_AUDIO,
    MEMORY_TAG_REGISTRY,
    MEMORY_TAG_PLUGIN,
    MEMORY_TAG_PLATFORM,
    MEMORY_TAG_SERIALIZER,
    MEMORY_TAG_ASSET,

    MEMORY_TAG_MAX_TAGS
} memory_tag;

/** @brief Configuration for the memory system. */
typedef struct memory_system_configuration
{
    // Total memory size in bytes used by the internal allocator for this system
    u64 total_alloc_size;
} memory_system_configuration;

BAPI b8 memory_system_initialize(memory_system_configuration config);
BAPI void memory_system_shutdown(void);

BAPI void* ballocate(u64 size, memory_tag tag);

#define BALLOC_TYPE(type, mem_tag) (type*)ballocate(sizeof(type), mem_tag)
#define BFREE_TYPE(block, type, mem_tag) bfree(block, sizeof(type), mem_tag)
#define BALLOC_TYPE_CARRAY(type, count) (type*)ballocate(sizeof(type) * count, MEMORY_TAG_ARRAY)
#define BFREE_TYPE_CARRAY(block, type, count) bfree(block, sizeof(type) * count, MEMORY_TAG_ARRAY)

BAPI void* ballocate_aligned(u64 size, u16 alignment, memory_tag tag);
BAPI void ballocate_report(u64 size, memory_tag tag);

BAPI void* breallocate(void* block, u64 old_size, u64 new_size, memory_tag tag);
#define BREALLOC_TYPE_CARRAY(block, type, old_count, new_count) (type*)breallocate(block, sizeof(type) * old_count, sizeof(type) * new_count, MEMORY_TAG_ARRAY)

BAPI void* breallocate_aligned(void* block, u64 old_size, u64 new_size, u16 alignment, memory_tag tag);
BAPI void breallocate_report(u64 old_size, u64 new_size, memory_tag tag);

BAPI void bfree(void* block, u64 size, memory_tag tag);
BAPI void bfree_aligned(void* block, u64 size, u16 alignment, memory_tag tag);
BAPI void bfree_report(u64 size, memory_tag tag);

BAPI b8 bmemory_get_size_alignment(void* block, u64* out_size, u16* out_alignment);

BAPI void* bzero_memory(void* block, u64 size);
BAPI void* bcopy_memory(void* dest, const void* source, u64 size);

#define BCOPY_TYPE(dest, source, type) bcopy_memory(dest, source, sizeof(type))
#define BCOPY_TYPE_CARRAY(dest, source, type, count) bcopy_memory(dest, source, sizeof(type) * count)

BAPI void* bset_memory(void* dest, i32 value, u64 size);

BAPI char* get_memory_usage_str(void);
BAPI u64 get_memory_alloc_count(void);

BAPI u32 pack_u8_into_u32(u8 x, u8 y, u8 z, u8 w);
BAPI b8 unpack_u8_from_u32(u32 n, u8* x, u8* y, u8* z, u8* w);
