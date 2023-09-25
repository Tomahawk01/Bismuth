#pragma once

#include "defines.h"

typedef struct ring_queue
{
    u32 length;
    u32 stride;
    u32 capacity;
    void* block;
    b8 owns_memory;
    i32 head;
    i32 tail;
} ring_queue;

b8 ring_queue_create(u32 stride, u32 capacity, void* memory, ring_queue* out_queue);
void ring_queue_destroy(ring_queue* queue);

b8 ring_queue_enqueue(ring_queue* queue, void* value);
b8 ring_queue_dequeue(ring_queue* queue, void* out_value);

b8 ring_queue_peek(const ring_queue* queue, void* out_value);
