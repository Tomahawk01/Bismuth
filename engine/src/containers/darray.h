#pragma once

#include "defines.h"

struct frame_allocator_int;

BAPI void* _darray_create(u64 length, u64 stride, struct frame_allocator_int* frame_allocator);
BAPI void* _darray_resize(void* array);
BAPI void* _darray_push(void* array, const void* value_ptr);
BAPI void* _darray_insert_at(void* array, u64 index, void* value_ptr);

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2

#define darray_create(type) \
    _darray_create(DARRAY_DEFAULT_CAPACITY, sizeof(type), 0)

#define darray_create_with_allocator(type, allocator) \
    _darray_create(DARRAY_DEFAULT_CAPACITY, sizeof(type), allocator)

#define darray_reserve(type, capacity) \
    _darray_create(capacity, sizeof(type), 0)

#define darray_reserve_with_allocator(type, capacity, allocator) \
    _darray_create(capacity, sizeof(type), allocator)

BAPI void darray_destroy(void* array);

#define darray_push(array, value)           \
    {                                       \
        typeof(value) temp = value;         \
        array = _darray_push(array, &temp); \
    }

BAPI void darray_pop(void* array, void* value_ptr);

#define darray_insert_at(array, index, value)           \
    {                                                   \
        typeof(value) temp = value;                     \
        array = _darray_insert_at(array, index, &temp); \
    }

BAPI void* darray_pop_at(void* array, u64 index, void* value_ptr);

BAPI void darray_clear(void* array);

BAPI u64 darray_capacity(void* array);
BAPI u64 darray_length(void* array);
BAPI u64 darray_stride(void* array);

BAPI void darray_length_set(void* array, u64 value);
