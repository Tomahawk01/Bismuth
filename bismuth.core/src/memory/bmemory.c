#include "memory/bmemory.h"

#include "debug/bassert.h"
#include "logger.h"
#include "strings/bstring.h"
#include "threads/bmutex.h"
#include "platform/platform.h"
#include "memory/allocators/dynamic_allocator.h"

// TODO: Custom string lib
#include <string.h>
#include <stdio.h>

#define B_USE_CUSTOM_MEMORY_ALLOCATOR 1

#if !B_USE_CUSTOM_MEMORY_ALLOCATOR
#if _MSC_VER
    #include <malloc.h>
    #define baligned_alloc _aligned_malloc
    #define baligned_free _aligned_free
#else
    #include <stdlib.h>
    #define baligned_alloc(size, alignment) aligned_alloc(alignment, size)
    #define baligned_free free
#endif
#endif

struct memory_stats
{
    u64 total_allocated;
    u64 tagged_allocations[MEMORY_TAG_MAX_TAGS];
    u64 new_tagged_allocations[MEMORY_TAG_MAX_TAGS];
    u64 new_tagged_deallocations[MEMORY_TAG_MAX_TAGS];
};

static const char* memory_tag_strings[MEMORY_TAG_MAX_TAGS] =
{
    "UNKNOWN     ",
    "ARRAY       ",
    "LINEAR_ALLOC",
    "DARRAY      ",
    "DICT        ",
    "RING_QUEUE  ",
    "BST         ",
    "STRING      ",
    "ENGINE      ",
    "JOB         ",
    "TEXTURE     ",
    "MAT_INST    ",
    "RENDERER    ",
    "GAME        ",
    "TRANSFORM   ",
    "ENTITY      ",
    "ENTITY_NODE ",
    "SCENE       ",
    "RESOURCE    ",
    "VULKAN      ",
    "VULKAN_EXT  ",
    "DIRECT3D    ",
    "OPENGL      ",
    "GPU_LOCAL   ",
    "BITMAP_FONT ",
    "SYSTEM_FONT ",
    "KEYMAP      ",
    "HASHTABLE   ",
    "UI          ",
    "AUDIO       ",
    "REGISTRY    ",
    "PLUGIN      ",
    "PLATFORM    ",
    "SERIALIZER  ",
    "ASSET       "
};

typedef struct memory_system_state
{
    memory_system_configuration config;
    struct memory_stats stats;
    u64 alloc_count;
    u64 allocator_memory_requirement;
    dynamic_allocator allocator;
    void* allocator_block;
    bmutex allocation_mutex;
} memory_system_state;

// Pointer to system state
static memory_system_state* state_ptr;

b8 memory_system_initialize(memory_system_configuration config)
{
#if B_USE_CUSTOM_MEMORY_ALLOCATOR
    // Amount needed by the system state
    u64 state_memory_requirement = sizeof(memory_system_state);

    // Figure out how much space dynamic allocator needs
    u64 alloc_requirement = 0;
    dynamic_allocator_create(config.total_alloc_size, &alloc_requirement, 0, 0);

    // Call platform allocator to get memory for the whole system, including state
    // TODO: memory alignment
    void* block = platform_allocate(state_memory_requirement + alloc_requirement, true);
    if (!block)
    {
        BFATAL("Memory system allocation failed and the system cannot continue");
        return false;
    }
    
    // State is in the first part of massive block of memory
    state_ptr = (memory_system_state*)block;
    state_ptr->config = config;
    state_ptr->alloc_count = 0;
    state_ptr->allocator_memory_requirement = alloc_requirement;
    platform_zero_memory(&state_ptr->stats, sizeof(state_ptr->stats));
    // Allocator block is in the same block of memory, but after the state
    state_ptr->allocator_block = ((void*)block + state_memory_requirement);

    if (!dynamic_allocator_create(
            config.total_alloc_size,
            &state_ptr->allocator_memory_requirement,
            state_ptr->allocator_block,
            &state_ptr->allocator))
    {
        BFATAL("Memory system is unable to setup internal allocator. Application cannot continue");
        return false;
    }
#else
    state_ptr = baligned_alloc(sizeof(memory_system_state), 16);
    state_ptr->config = config;
    state_ptr->alloc_count = 0;
    state_ptr->allocator_memory_requirement = 0;
#endif

    // Create allocation mutex
    if (!bmutex_create(&state_ptr->allocation_mutex))
    {
        BFATAL("Unable to create allocation mutex");
        return false;
    }

    BDEBUG("Memory system successfully allocated %llu bytes", config.total_alloc_size);
    return true;
}

void memory_system_shutdown(void)
{
    if (state_ptr)
    {
        // Destroy allocation mutex
        bmutex_destroy(&state_ptr->allocation_mutex);

#if B_USE_CUSTOM_MEMORY_ALLOCATOR
        dynamic_allocator_destroy(&state_ptr->allocator);
        // Free entire block
        platform_free(state_ptr, state_ptr->allocator_memory_requirement + sizeof(memory_system_state));
#else
        baligned_free(state_ptr);
#endif
    }
    state_ptr = 0;
}

void* ballocate(u64 size, memory_tag tag)
{
    return ballocate_aligned(size, 1, tag);
}

void* ballocate_aligned(u64 size, u16 alignment, memory_tag tag)
{
    BASSERT_MSG(size, "ballocate_aligned requires a nonzero size");
    if (tag == MEMORY_TAG_UNKNOWN)
        BWARN("ballocate_aligned called using MEMORY_TAG_UNKNOWN. Re-class this allocation");

    // Either allocate from the system's allocator or the OS
    void* block = 0;
    if (state_ptr)
    {
        // Make sure multithreaded requests don't violate each other
        if (!bmutex_lock(&state_ptr->allocation_mutex))
        {
            BFATAL("Error obtaining mutex lock during allocation");
            return 0;
        }

        state_ptr->stats.total_allocated += size;
        state_ptr->stats.tagged_allocations[tag] += size;
        state_ptr->stats.new_tagged_allocations[tag] += size;
        state_ptr->alloc_count++;

#if B_USE_CUSTOM_MEMORY_ALLOCATOR
        block = dynamic_allocator_allocate_aligned(&state_ptr->allocator, size, alignment);
#else
        block = baligned_alloc(size, alignment);
#endif
        bmutex_unlock(&state_ptr->allocation_mutex);
    }
    else
    {
        // If system is not up yet, warn about it but give memory for now
        // BWARN("ballocate_aligned called before the memory system is initialized");
        // TODO: Memory alignment
        block = platform_allocate(size, false);
    }

    if (block)
    {
        platform_zero_memory(block, size);
        return block;
    }

    BFATAL("ballocate_aligned failed to allocate");
    return 0;
}

void ballocate_report(u64 size, memory_tag tag)
{
    // Make sure multithreaded requests don't trample each other
    if (!bmutex_lock(&state_ptr->allocation_mutex))
    {
        BFATAL("Error obtaining mutex lock during allocation reporting");
        return;
    }
    state_ptr->stats.total_allocated += size;
    state_ptr->stats.tagged_allocations[tag] += size;
    state_ptr->stats.new_tagged_allocations[tag] += size;
    state_ptr->alloc_count++;
    bmutex_unlock(&state_ptr->allocation_mutex);
}

void* breallocate(void* block, u64 old_size, u64 new_size, memory_tag tag)
{
    return breallocate_aligned(block, old_size, new_size, 1, tag);
}

void* breallocate_aligned(void* block, u64 old_size, u64 new_size, u16 alignment, memory_tag tag)
{
    void* new_block = ballocate_aligned(new_size, alignment, tag);
    if (block && new_block)
    {
        bcopy_memory(new_block, block, old_size);
        bfree_aligned(block, old_size, alignment, tag);
    }
    return new_block;
}

void breallocate_report(u64 old_size, u64 new_size, memory_tag tag)
{
    bfree_report(old_size, tag);
    ballocate_report(new_size, tag);
}

void bfree(void* block, u64 size, memory_tag tag)
{
    bfree_aligned(block, size, 1, tag);
}

void bfree_aligned(void* block, u64 size, u16 alignment, memory_tag tag)
{
    if (tag == MEMORY_TAG_UNKNOWN)
        BWARN("bfree_aligned called using MEMORY_TAG_UNKNOWN. Re-class this allocation");

    if (state_ptr)
    {
        // Make sure multithreaded requests don't violate each other
        if (!bmutex_lock(&state_ptr->allocation_mutex))
        {
            BFATAL("Unable to obtain mutex lock for free operation");
            return;
        }

#if B_USE_CUSTOM_MEMORY_ALLOCATOR
        u64 osize = 0;
        u16 oalignment = 0;
        dynamic_allocator_get_size_alignment(&state_ptr->allocator, block, &osize, &oalignment);
        if (osize != size)
            printf("Free size mismatch! (original=%llu, requested=%llu)\n", osize, size);
        if (oalignment != alignment)
            printf("Free alignment mismatch! (original=%hu, requested=%hu)\n", oalignment, alignment);
#endif

        state_ptr->stats.total_allocated -= size;
        state_ptr->stats.tagged_allocations[tag] -= size;
        state_ptr->stats.new_tagged_deallocations[tag] += size;
        state_ptr->alloc_count--;
#if B_USE_CUSTOM_MEMORY_ALLOCATOR
        b8 result = dynamic_allocator_free_aligned(&state_ptr->allocator, block);
#else
        baligned_free(block);
        b8 result = true;
#endif

        bmutex_unlock(&state_ptr->allocation_mutex);

        if (!result)
        {
            // TODO: Memory alignment
            platform_free(block, false);
        }
    }
    else
    {
        // TODO: Memory alignment
        platform_free(block, false);
    }
}

void bfree_report(u64 size, memory_tag tag)
{
    // Make sure multithreaded requests don't trample each other
    if (!bmutex_lock(&state_ptr->allocation_mutex))
    {
        BFATAL("Error obtaining mutex lock during allocation reporting");
        return;
    }
    state_ptr->stats.total_allocated -= size;
    state_ptr->stats.tagged_allocations[tag] -= size;
    state_ptr->stats.new_tagged_deallocations[tag] += size;
    state_ptr->alloc_count--;
    bmutex_unlock(&state_ptr->allocation_mutex);
}

b8 bmemory_get_size_alignment(void* block, u64* out_size, u16* out_alignment)
{
    if (!bmutex_lock(&state_ptr->allocation_mutex))
    {
        BFATAL("Error obtaining mutex lock during bmemory_get_size_alignment");
        return false;
    }
#if B_USE_CUSTOM_MEMORY_ALLOCATOR
    b8 result = dynamic_allocator_get_size_alignment(&state_ptr->allocator, block, out_size, out_alignment);
#else
    *out_size = 0;
    *out_alignment = 1;
    b8 result = true;
#endif
    bmutex_unlock(&state_ptr->allocation_mutex);
    return result;
}

void* bzero_memory(void* block, u64 size)
{
    return platform_zero_memory(block, size);
}

void* bcopy_memory(void* dest, const void* source, u64 size)
{
    return platform_copy_memory(dest, source, size);
}

void* bset_memory(void* dest, i32 value, u64 size)
{
    return platform_set_memory(dest, value, size);
}

const char* get_unit_for_size(u64 size_bytes, f32* out_amount)
{
    // gibibyte, mebibyte, kibibyte (more technically correct)
    if (size_bytes >= GIBIBYTES(1)) {
        *out_amount = (f64)size_bytes / GIBIBYTES(1);
        return "GiB";
    } else if (size_bytes >= MEBIBYTES(1)) {
        *out_amount = (f64)size_bytes / MEBIBYTES(1);
        return "MiB";
    } else if (size_bytes >= KIBIBYTES(1)) {
        *out_amount = (f64)size_bytes / KIBIBYTES(1);
        return "KiB";
    } else {
        *out_amount = (f32)size_bytes;
        return "B";
    }
}

char* get_memory_usage_str(void)
{
    char buffer[8000] = "System memory use (tagged):\n";
    u64 offset = strlen(buffer);
    for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i)
    {
        f32 amounts[3] = {1.0f, 1.0f, 1.0f};
        const char* units[3] = {
            get_unit_for_size(state_ptr->stats.tagged_allocations[i], &amounts[0]),
            get_unit_for_size(state_ptr->stats.new_tagged_allocations[i], &amounts[1]),
            get_unit_for_size(state_ptr->stats.new_tagged_deallocations[i], &amounts[2])
        };

        i32 length = snprintf(buffer + offset, 8000, "  %s: %-7.2f %-3s [+ %-7.2f %-3s | - %-7.2f%-3s]\n",
                              memory_tag_strings[i], amounts[0], units[0], amounts[1], units[1], amounts[2], units[2]);
        offset += length;
    }

    bzero_memory(&state_ptr->stats.new_tagged_allocations, sizeof(state_ptr->stats.new_tagged_allocations));
    bzero_memory(&state_ptr->stats.new_tagged_deallocations, sizeof(state_ptr->stats.new_tagged_deallocations));

    {
        // Compute total usage
#if B_USE_CUSTOM_MEMORY_ALLOCATOR
        u64 total_space = dynamic_allocator_total_space(&state_ptr->allocator);
        u64 free_space = dynamic_allocator_free_space(&state_ptr->allocator);
        u64 used_space = total_space - free_space;
#else
        u64 total_space = 0;
        // u64 free_space = 0;
        u64 used_space = 0;
#endif

        f32 used_amount = 1.0f;
        const char* used_unit = get_unit_for_size(used_space, &used_amount);

        f32 total_amount = 1.0f;
        const char* total_unit = get_unit_for_size(total_space, &total_amount);

        f64 percent_used = (f64)(used_space) / total_space;

        i32 length = snprintf(buffer + offset, 8000, "Total memory usage: %.2f%s of %.2f%s (%.2f%%)\n", used_amount, used_unit, total_amount, total_unit, percent_used);
        offset += length;
    }

    char* out_string = string_duplicate(buffer);
    return out_string;
}

u64 get_memory_alloc_count(void)
{
    if (state_ptr)
        return state_ptr->alloc_count;
    return 0;
}

u32 pack_u8_into_u32(u8 x, u8 y, u8 z, u8 w)
{
    return (x << 24) | (y << 16) | (z << 8) | (w);
}

b8 unpack_u8_from_u32(u32 n, u8* x, u8* y, u8* z, u8* w)
{
    if (!x || !y || !z || !w)
        return false;

    *x = (n >> 24) & 0xFF;
    *y = (n >> 16) & 0xFF;
    *z = (n >> 8) & 0xFF;
    *w = n & 0xFF;
    
    return true;
}
