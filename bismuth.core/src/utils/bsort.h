#pragma once

#include "defines.h"

typedef i32 (*PFN_bquicksort_compare)(void* a, void* b);

BAPI void ptr_swap(void* scratch_mem, u64 size, void* a, void* b);

BAPI void bquick_sort(u64 type_size, void* data, i32 low_index, i32 high_index, PFN_bquicksort_compare compare_pfn);

BAPI i32 bquicksort_compare_u32_desc(void* a, void* b);
BAPI i32 bquicksort_compare_u32(void* a, void* b);
