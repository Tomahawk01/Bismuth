#include "queue.h"

#include "memory/bmemory.h"
#include "logger.h"

static void queue_ensure_allocated(queue* s, u32 count)
{
    if (s->allocated < s->element_size * count)
    {
        void* temp = ballocate(count * s->element_size, MEMORY_TAG_ARRAY);
        if (s->memory)
        {
            bcopy_memory(temp, s->memory, s->allocated);
            bfree(s->memory, s->allocated, MEMORY_TAG_ARRAY);
        }
        s->memory = temp;
        s->allocated = count * s->element_size;
    }
}

b8 queue_create(queue* out_queue, u32 element_size)
{
    if (!out_queue)
    {
        BERROR("queue_create requires a pointer to a valid queue");
        return false;
    }

    bzero_memory(out_queue, sizeof(queue));
    out_queue->element_size = element_size;
    out_queue->element_count = 0;
    queue_ensure_allocated(out_queue, 1);
    return true;
}

void queue_destroy(queue* s)
{
    if (s)
    {
        if (s->memory)
            bfree(s->memory, s->allocated, MEMORY_TAG_ARRAY);
        bzero_memory(s, sizeof(queue));
    }
}

b8 queue_push(queue* s, void* element_data)
{
    if (!s)
    {
        BERROR("queue_push requires a pointer to a valid queue");
        return false;
    }

    queue_ensure_allocated(s, s->element_count + 1);
    bcopy_memory((void*)((u64)s->memory + (s->element_count * s->element_size)), element_data, s->element_size);
    s->element_count++;
    return true;
}

b8 queue_peek(const queue* s, void* out_element_data)
{
    if (!s || !out_element_data)
    {
        BERROR("queue_peek requires a pointer to a valid queue and to hold element data output");
        return false;
    }

    if (s->element_count < 1)
    {
        BWARN("Cannot peek from an empty queue");
        return false;
    }

    // Copy the front entry to out_element_data
    bcopy_memory(out_element_data, s->memory, s->element_size);

    return true;
}

b8 queue_pop(queue* s, void* out_element_data)
{
    if (!s || !out_element_data)
    {
        BERROR("queue_pop requires a pointer to a valid queue and to hold element data output");
        return false;
    }

    if (s->element_count < 1)
    {
        BWARN("Cannot pop from an empty queue");
        return false;
    }

    // Copy the front entry to out_element_data
    bcopy_memory(out_element_data, s->memory, s->element_size);

    // Move everything forward
    bcopy_memory(s->memory, (void*)(((u64)s->memory) + s->element_size), s->element_size * (s->element_count - 1));

    s->element_count--;

    return true;
}
