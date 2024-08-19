#pragma once

#include "defines.h"

typedef struct queue
{
    // Element size in bytes
    u32 element_size;
    // Current element count
    u32 element_count;
    // Total amount of currently-allocated memory
    u32 allocated;
    // Allocated memory block
    void* memory;
} queue;

BAPI b8 queue_create(queue* out_queue, u32 element_size);
BAPI void queue_destroy(queue* s);

BAPI b8 queue_push(queue* s, void* element_data);
BAPI b8 queue_peek(const queue* s, void* out_element_data);
BAPI b8 queue_pop(queue* s, void* out_element_data);
