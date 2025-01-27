#pragma once

#include "defines.h"

typedef struct darray_header
{
    u64 capacity;
    u64 length;
    u64 stride;
    struct frame_allocator_int* allocator;
} darray_header;

BAPI void* _darray_create(u64 length, u64 stride, struct frame_allocator_int* frame_allocator);
BAPI void* _darray_resize(void* array);
BAPI void* _darray_push(void* array, const void* value_ptr);
BAPI void* _darray_insert_at(void* array, u64 index, void* value_ptr);
BAPI void* _darray_duplicate(u64 stride, void* array);

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2

#define darray_create(type) \
    (type*)_darray_create(DARRAY_DEFAULT_CAPACITY, sizeof(type), 0)

#define darray_create_with_allocator(type, allocator) \
    _darray_create(DARRAY_DEFAULT_CAPACITY, sizeof(type), allocator)

#define darray_reserve(type, capacity) \
    _darray_create(capacity, sizeof(type), 0)

#define darray_reserve_with_allocator(type, capacity, allocator) \
    _darray_create(capacity, sizeof(type), allocator)

BAPI void darray_destroy(void* array);

#define darray_push(array, value)                       \
    {                                                   \
        typeof(value) __b_temp_value__ = value;         \
        array = _darray_push(array, &__b_temp_value__); \
    }

BAPI void darray_pop(void* array, void* dest);

#define darray_insert_at(array, index, value)                       \
    {                                                               \
        typeof(value) __b_temp_value__ = value;                     \
        array = _darray_insert_at(array, index, &__b_temp_value__); \
    }

BAPI void* darray_pop_at(void* array, u64 index, void* dest);

BAPI void darray_clear(void* array);

BAPI u64 darray_capacity(void* array);
BAPI u64 darray_length(void* array);
BAPI u64 darray_stride(void* array);

BAPI void darray_length_set(void* array, u64 value);

#define darray_duplicate(type, array) (type*)_darray_duplicate(sizeof(type), array)

/**
 * NEW DARRAY
 */

BAPI void _bdarray_init(u32 length, u32 stride, u32 capacity, struct frame_allocator_int* allocator, u32* out_length, u32* out_stride, u32* out_capacity, void** block, struct frame_allocator_int** out_allocator);
BAPI void _bdarray_free(u32* length, u32* capacity, u32* stride, void** block, struct frame_allocator_int** out_allocator);
BAPI void _bdarray_ensure_size(u32 required_length, u32 stride, u32* out_capacity, struct frame_allocator_int* allocator, void** block, void** base_block);

typedef struct darray_base
{
    u32 length;
    u32 stride;
    u32 capacity;
    struct frame_allocator_int* allocator;
    void* p_data;
} darray_base;

typedef struct darray_iterator
{
    darray_base* arr;
    i32 pos;
    i32 dir;
    b8 (*end)(const struct darray_iterator* it);
    void* (*value)(const struct darray_iterator* it);
    void (*next)(struct darray_iterator* it);
    void (*prev)(struct darray_iterator* it);
} darray_iterator;

BAPI darray_iterator darray_iterator_begin(darray_base* arr);
BAPI darray_iterator darray_iterator_rbegin(darray_base* arr);
BAPI b8 darray_iterator_end(const darray_iterator* it);
BAPI void* darray_iterator_value(const darray_iterator* it);
BAPI void darray_iterator_next(darray_iterator* it);
BAPI void darray_iterator_prev(darray_iterator* it);

#define DARRAY_TYPE_NAMED(type, name)                                                                                                                                       \
    typedef struct darray_##name {                                                                                                                                          \
        darray_base base;                                                                                                                                                   \
        type* data;                                                                                                                                                         \
        darray_iterator (*begin)(darray_base * arr);                                                                                                                        \
        darray_iterator (*rbegin)(darray_base * arr);                                                                                                                       \
    } darray_##name;                                                                                                                                                        \
                                                                                                                                                                            \
    BINLINE darray_##name darray_##name##_reserve_with_allocator(u32 capacity, struct frame_allocator_int* allocator) {                                                     \
        darray_##name arr;                                                                                                                                                  \
        _bdarray_init(0, sizeof(type), capacity, allocator, &arr.base.length, &arr.base.stride, &arr.base.capacity, (void**)&arr.data, &arr.base.allocator);                \
        arr.base.p_data = arr.data;                                                                                                                                         \
        arr.begin = darray_iterator_begin;                                                                                                                                  \
        arr.rbegin = darray_iterator_rbegin;                                                                                                                                \
        return arr;                                                                                                                                                         \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE darray_##name darray_##name##_create_with_allocator(struct frame_allocator_int* allocator) {                                                                    \
        darray_##name arr;                                                                                                                                                  \
        _bdarray_init(0, sizeof(type), DARRAY_DEFAULT_CAPACITY, allocator, &arr.base.length, &arr.base.stride, &arr.base.capacity, (void**)&arr.data, &arr.base.allocator); \
        arr.base.p_data = arr.data;                                                                                                                                         \
        arr.begin = darray_iterator_begin;                                                                                                                                  \
        arr.rbegin = darray_iterator_rbegin;                                                                                                                                \
        return arr;                                                                                                                                                         \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE darray_##name darray_##name##_reserve(u32 capacity) {                                                                                                           \
        darray_##name arr;                                                                                                                                                  \
        _bdarray_init(0, sizeof(type), capacity, 0, &arr.base.length, &arr.base.stride, &arr.base.capacity, (void**)&arr.data, &arr.base.allocator);                        \
        arr.base.p_data = arr.data;                                                                                                                                         \
        arr.begin = darray_iterator_begin;                                                                                                                                  \
        arr.rbegin = darray_iterator_rbegin;                                                                                                                                \
        return arr;                                                                                                                                                         \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE darray_##name darray_##name##_create(void) {                                                                                                                    \
        darray_##name arr;                                                                                                                                                  \
        _bdarray_init(0, sizeof(type), DARRAY_DEFAULT_CAPACITY, 0, &arr.base.length, &arr.base.stride, &arr.base.capacity, (void**)&arr.data, &arr.base.allocator);         \
        arr.base.p_data = arr.data;                                                                                                                                         \
        arr.begin = darray_iterator_begin;                                                                                                                                  \
        arr.rbegin = darray_iterator_rbegin;                                                                                                                                \
        return arr;                                                                                                                                                         \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE darray_##name* darray_##name##_push(darray_##name* arr, type data) {                                                                                            \
        _bdarray_ensure_size(arr->base.length + 1, arr->base.stride, &arr->base.capacity, arr->base.allocator, (void**)&arr->data, (void**)&arr->base.p_data);              \
        arr->data[arr->base.length] = data;                                                                                                                                 \
        arr->base.length++;                                                                                                                                                 \
        return arr;                                                                                                                                                         \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE b8 darray_##name##_pop(darray_##name* arr, type* out_value) {                                                                                                   \
        if (arr->base.length < 1) {                                                                                                                                         \
            return false;                                                                                                                                                   \
        }                                                                                                                                                                   \
        *out_value = arr->data[arr->base.length - 1];                                                                                                                       \
        arr->base.length--;                                                                                                                                                 \
        return true;                                                                                                                                                        \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE b8 darray_##name##_pop_at(darray_##name* arr, u32 index, type* out_value) {                                                                                     \
        if (index >= arr->base.length) {                                                                                                                                    \
            return false;                                                                                                                                                   \
        }                                                                                                                                                                   \
        *out_value = arr->data[index];                                                                                                                                      \
        for (u32 i = index; i < arr->base.length; ++i) {                                                                                                                    \
            arr->data[i] = arr->data[i + 1];                                                                                                                                \
        }                                                                                                                                                                   \
        arr->base.length--;                                                                                                                                                 \
        return true;                                                                                                                                                        \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE b8 darray_##name##_insert_at(darray_##name* arr, u32 index, type data) {                                                                                        \
        if (index > arr->base.length) {                                                                                                                                     \
            return false;                                                                                                                                                   \
        }                                                                                                                                                                   \
        _bdarray_ensure_size(arr->base.length + 1, arr->base.stride, &arr->base.capacity, arr->base.allocator, (void**)&arr->data, (void**)&arr->base.p_data);              \
        arr->base.length++;                                                                                                                                                 \
        for (u32 i = arr->base.length; i > index; --i) {                                                                                                                    \
            arr->data[i] = arr->data[i - 1];                                                                                                                                \
        }                                                                                                                                                                   \
        arr->data[index] = data;                                                                                                                                            \
        return true;                                                                                                                                                        \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE darray_##name* darray_##name##_clear(darray_##name* arr) {                                                                                                      \
        arr->base.length = 0;                                                                                                                                               \
        return arr;                                                                                                                                                         \
    }                                                                                                                                                                       \
                                                                                                                                                                            \
    BINLINE void darray_##name##_destroy(darray_##name* arr) {                                                                                                              \
        _bdarray_free(&arr->base.length, &arr->base.capacity, &arr->base.stride, (void**)&arr->data, &arr->base.allocator);                                                 \
        arr->begin = 0;                                                                                                                                                     \
        arr->rbegin = 0;                                                                                                                                                    \
    }

// Create an array type of the given type. For advanced types or pointers, use ARRAY_TYPE_NAMED directly
#define DARRAY_TYPE(type) DARRAY_TYPE_NAMED(type, type)

// Create array types for well-known types

DARRAY_TYPE(b8);

DARRAY_TYPE(u8);
DARRAY_TYPE(u16);
DARRAY_TYPE(u32);
DARRAY_TYPE(u64);

DARRAY_TYPE(i8);
DARRAY_TYPE(i16);
DARRAY_TYPE(i32);
DARRAY_TYPE(i64);

DARRAY_TYPE(f32);
DARRAY_TYPE(f64);

// Create array types for well-known "advanced" types, such as strings
DARRAY_TYPE_NAMED(const char*, string);
