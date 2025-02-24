#include "bsort.h"

#include "memory/bmemory.h"

void ptr_swap(void* scratch_mem, u64 size, void* a, void* b)
{
    bcopy_memory(scratch_mem, a, size);
    bcopy_memory(a, b, size);
    bcopy_memory(b, scratch_mem, size);
}

static void* data_at_index(void* block, u64 element_size, u32 index)
{
    u64 addr = (u64)block;
    addr += (element_size * index);
    return (void*)(addr);
}

static i32 bquick_sort_partition(void* scratch_mem, u64 size, void* data, i32 low_index, i32 high_index, PFN_bquicksort_compare compare_pfn)
{
    void* pivot = data_at_index(data, size, high_index);
    i32 i = (low_index - 1);

    for (i32 j = low_index; j <= high_index - 1; ++j)
    {
        void* dataj = data_at_index(data, size, j);
        i32 result = compare_pfn(dataj, pivot);
        if (result > 0)
        {
            ++i;
            void* datai = data_at_index(data, size, i);
            ptr_swap(scratch_mem, size, datai, dataj);
        }
    }
    ptr_swap(scratch_mem, size, data_at_index(data, size, i + 1), data_at_index(data, size, high_index));
    return i + 1;
}

static void bquick_sort_internal(void* scratch_mem, u64 size, void* data, i32 low_index, i32 high_index, PFN_bquicksort_compare compare_pfn)
{
    if (low_index < high_index)
    {
        i32 partition_index = bquick_sort_partition(scratch_mem, size, data, low_index, high_index, compare_pfn);
        bquick_sort_internal(scratch_mem, size, data, low_index, partition_index - 1, compare_pfn);
        bquick_sort_internal(scratch_mem, size, data, partition_index + 1, high_index, compare_pfn);
    }
}

void bquick_sort(u64 type_size, void* data, i32 low_index, i32 high_index, PFN_bquicksort_compare compare_pfn)
{
    if (low_index < high_index)
    {
        void* scratch_mem = ballocate(type_size, MEMORY_TAG_ARRAY);
        i32 partition_index = bquick_sort_partition(scratch_mem, type_size, data, low_index, high_index, compare_pfn);
        bquick_sort_internal(scratch_mem, type_size, data, low_index, partition_index - 1, compare_pfn);
        bquick_sort_internal(scratch_mem, type_size, data, partition_index + 1, high_index, compare_pfn);
        bfree(scratch_mem, type_size, MEMORY_TAG_ARRAY);
    }
}

i32 bquicksort_compare_u32_desc(void* a, void* b)
{
    u32 a_typed = *(u32*)a;
    u32 b_typed = *(u32*)b;
    if (a_typed > b_typed)
    {
        return 1;
    }
    else if (a_typed < b_typed)
    {
        return -1;
    }

    return 0;
}

i32 bquicksort_compare_u32(void* a, void* b)
{
    u32 a_typed = *(u32*)a;
    u32 b_typed = *(u32*)b;
    if (a_typed < b_typed)
    {
        return 1;
    }
    else if (a_typed > b_typed)
    {
        return -1;
    }

    return 0;
}
