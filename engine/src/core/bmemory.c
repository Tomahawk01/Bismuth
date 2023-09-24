#include "bmemory.h"
#include "core/logger.h"
#include "core/bstring.h"
#include "core/bmutex.h"
#include "platform/platform.h"
#include "memory/dynamic_allocator.h"

// TODO: Custom string lib
#include <string.h>
#include <stdio.h>

struct memory_stats
{
    u64 total_allocated;
    u64 tagged_allocations[MEMORY_TAG_MAX_TAGS];
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
    "APPLICATION ",
    "JOB         ",
    "TEXTURE     ",
    "MAT_INST    ",
    "RENDERER    ",
    "GAME        ",
    "TRANSFORM   ",
    "ENTITY      ",
    "ENTITY_NODE ",
    "SCENE       ",
    "RESOURCE    "
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
    // Amount needed by the system state
    u64 state_memory_requirement = sizeof(memory_system_state);

    // Figure out how much space dynamic allocator needs
    u64 alloc_requirement = 0;
    dynamic_allocator_create(config.total_alloc_size, &alloc_requirement, 0, 0);

    // Call platform allocator to get memory for the whole system, including state
    // TODO: memory alignment
    void* block = platform_allocate(state_memory_requirement + alloc_requirement, false);
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

    // Create allocation mutex
    if (!bmutex_create(&state_ptr->allocation_mutex))
    {
        BFATAL("Unable to create allocation mutex");
        return false;
    }

    BDEBUG("Memory system successfully allocated %llu bytes", config.total_alloc_size);
    return true;
}

void memory_system_shutdown()
{
    if (state_ptr)
    {
        // Destroy allocation mutex
        bmutex_destroy(&state_ptr->allocation_mutex);

        dynamic_allocator_destroy(&state_ptr->allocator);
        // Free entire block
        platform_free(state_ptr, state_ptr->allocator_memory_requirement + sizeof(memory_system_state));
    }
    state_ptr = 0;
}

void* ballocate(u64 size, memory_tag tag)
{
    if (tag == MEMORY_TAG_UNKNOWN)
        BWARN("ballocate called using MEMORY_TAG_UNKNOWN. Re-class this allocation");

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
        state_ptr->alloc_count++;

        block = dynamic_allocator_allocate(&state_ptr->allocator, size);
        bmutex_unlock(&state_ptr->allocation_mutex);
    }
    else
    {
        // If system is not up yet, warn about it but give memory for now
        BWARN("ballocate called before the memory system is initialized");
        // TODO: Memory alignment
        block = platform_allocate(size, false);
    }

    if (block)
    {
        platform_zero_memory(block, size);
        return block;
    }

    BFATAL("ballocate failed to allocate");
    return 0;
}

void bfree(void* block, u64 size, memory_tag tag)
{
    if (tag == MEMORY_TAG_UNKNOWN)
        BWARN("bfree called using MEMORY_TAG_UNKNOWN. Re-class this allocation");

    if (state_ptr)
    {
        // Make sure multithreaded requests don't violate each other
        if (!bmutex_lock(&state_ptr->allocation_mutex))
        {
            BFATAL("Unable to obtain mutex lock for free operation");
            return;
        }

        state_ptr->stats.total_allocated -= size;
        state_ptr->stats.tagged_allocations[tag] -= size;
        b8 result = dynamic_allocator_free(&state_ptr->allocator, block, size);

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

char* get_memory_usage_str()
{
    // gibibyte, mebibyte, kibibyte (more technically correct)
    const u64 gib = 1024 * 1024 * 1024;
    const u64 mib = 1024 * 1024;
    const u64 kib = 1024;

    char buffer[8000] = "System memory use (tagged):\n";
    u64 offset = strlen(buffer);
    for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i)
    {
        char unit[4] = "XiB";
        float amount = 1.0f;
        if (state_ptr->stats.tagged_allocations[i] >= gib)
        {
            unit[0] = 'G';
            amount = state_ptr->stats.tagged_allocations[i] / (float)gib;
        }
        else if (state_ptr->stats.tagged_allocations[i] >= mib)
        {
            unit[0] = 'M';
            amount = state_ptr->stats.tagged_allocations[i] / (float)mib;
        }
        else if (state_ptr->stats.tagged_allocations[i] >= kib)
        {
            unit[0] = 'K';
            amount = state_ptr->stats.tagged_allocations[i] / (float)kib;
        }
        else
        {
            unit[0] = 'B';
            unit[1] = 0;
            amount = (float)state_ptr->stats.tagged_allocations[i];
        }

        i32 length = snprintf(buffer + offset, 8000, "  %s: %.2f%s\n", memory_tag_strings[i], amount, unit);
        offset += length;
    }

    char* out_string = string_duplicate(buffer);
    return out_string;
}

u64 get_memory_alloc_count()
{
    if (state_ptr)
        return state_ptr->alloc_count;
    return 0;
}
