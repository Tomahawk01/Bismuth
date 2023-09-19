#pragma once

#include "defines.h"

typedef struct dynamic_allocator
{
    void* memory;
} dynamic_allocator;

BAPI b8 dynamic_allocator_create(u64 total_size, u64* memory_requirement, void* memory, dynamic_allocator* out_allocator);

BAPI b8 dynamic_allocator_destroy(dynamic_allocator* allocator);

BAPI void* dynamic_allocator_allocate(dynamic_allocator* allocator, u64 size);

BAPI b8 dynamic_allocator_free(dynamic_allocator* allocator, void* block, u64 size);

BAPI u64 dynamic_allocator_free_space(dynamic_allocator* allocator);
