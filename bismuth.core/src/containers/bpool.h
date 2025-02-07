#pragma once

#include "defines.h"

typedef struct bpool
{
    u32 element_size;
    u32 capacity;
    u32 allocated_count;
    void* block;
} bpool;

BAPI b8 bpool_create(u32 element_size, u32 capacity, bpool* out_pool);
BAPI void bpool_destroy(bpool* pool);
BAPI void* bpool_allocate(bpool* pool, u32* out_index);
BAPI void bpool_free(bpool* pool, void* element);
BAPI void bpool_free_by_index(bpool* pool, u32 index);
BAPI void* bpool_get_by_index(bpool* pool, u32 index);
