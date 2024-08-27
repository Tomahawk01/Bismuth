#pragma once

#include "defines.h"

BAPI void _barray_init(u32 length, u32 stride, u32* out_length, u32* out_stride, void** block);
BAPI void _barray_free(u32* length, u32* stride, void** block);

typedef struct array_base
{
    u32 length;
    u32 stride;
    void* p_data;
} array_base;

typedef struct array_iterator
{
    array_base* arr;
    i32 pos;
    i32 dir;
    b8 (*end)(const struct array_iterator* it);
    void* (*value)(const struct array_iterator* it);
    void (*next)(struct array_iterator* it);
    void (*prev)(struct array_iterator* it);
} array_iterator;

BAPI array_iterator array_iterator_begin(array_base* arr);
BAPI array_iterator array_iterator_rbegin(array_base* arr);
BAPI b8 array_iterator_end(const array_iterator* it);
BAPI void* array_iterator_value(const array_iterator* it);
BAPI void array_iterator_next(array_iterator* it);
BAPI void array_iterator_prev(array_iterator* it);

#define ARRAY_TYPE_NAMED(type, name)                                                                                     \
    struct array_##name;                                                                                                 \
    typedef struct array_##name                                                                                          \
    {                                                                                                                    \
        array_base base;                                                                                                 \
        type* data;                                                                                                      \
        array_iterator (*begin)(array_base * arr);                                                                       \
        array_iterator (*rbegin)(array_base * arr);                                                                      \
    } array_##name;                                                                                                      \
                                                                                                                         \
    BINLINE type* array_##name##_it_value(const array_iterator* it) { return &((array_##name*)it->arr)->data[it->pos]; } \
                                                                                                                         \
    BINLINE array_##name array_##name##_create(u32 length)                                                               \
    {                                                                                                                    \
        array_##name arr;                                                                                                \
        _barray_init(length, sizeof(type), &arr.base.length, &arr.base.stride, (void**)&arr.data);                       \
        arr.base.p_data = arr.data;                                                                                      \
        arr.begin = array_iterator_begin;                                                                                \
        arr.rbegin = array_iterator_rbegin;                                                                              \
        return arr;                                                                                                      \
    }                                                                                                                    \
                                                                                                                         \
    BINLINE void array_##name##_destroy(array_##name* arr)                                                               \
    {                                                                                                                    \
        _barray_free(&arr->base.length, &arr->base.stride, (void**)&arr->data);                                          \
        arr->begin = 0;                                                                                                  \
        arr->rbegin = 0;                                                                                                 \
    }

/** @brief Create an array type of the given type. For advanced types or pointers, use ARRAY_TYPE_NAMED directly */
#define ARRAY_TYPE(type) ARRAY_TYPE_NAMED(type, type)

// Create array types for well-known types

ARRAY_TYPE(b8);

ARRAY_TYPE(u8);
ARRAY_TYPE(u16);
ARRAY_TYPE(u32);
ARRAY_TYPE(u64);

ARRAY_TYPE(i8);
ARRAY_TYPE(i16);
ARRAY_TYPE(i32);
ARRAY_TYPE(i64);

ARRAY_TYPE(f32);
ARRAY_TYPE(f64);

// Create array types for well-known "advanced" types, such as strings

ARRAY_TYPE_NAMED(const char*, string);
