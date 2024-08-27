#include "array.h"

#include "memory/bmemory.h"

void _barray_init(u32 length, u32 stride, u32* out_length, u32* out_stride, void** block)
{
    *out_length = length;
    *out_stride = stride;
    *block = ballocate_aligned(length * stride, 16, MEMORY_TAG_ARRAY);
}

void _barray_free(u32* length, u32* stride, void** block)
{
    if (block && *block)
    {
        bfree_aligned(*block, (*length) * (*stride), 16, MEMORY_TAG_ARRAY);
        *length = 0;
        *stride = 0;
        *block = 0;
    }
}
