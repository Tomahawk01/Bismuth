#pragma once

#include "defines.h"

BAPI void _barray_init(u32 length, u32 stride, u32* out_length, u32* out_stride, void** block);
BAPI void _barray_free(u32* length, u32* stride, void** block);

#define ARRAY_TYPE_NAMED(type, name)                                                                         \
    typedef struct name##_array                                                                              \
    {                                                                                                        \
        u32 length;                                                                                          \
        u32 stride;                                                                                          \
        type* data;                                                                                          \
    } name##_array;                                                                                          \
    typedef struct name##_array_it                                                                           \
    {                                                                                                        \
        name##_array* arr;                                                                                   \
        u32 pos;                                                                                             \
    } name##_array_it;                                                                                       \
                                                                                                             \
    BINLINE name##_array name##_array_create(u32 length)                                                     \
    {                                                                                                        \
        name##_array arr;                                                                                    \
        _barray_init(length, sizeof(type), &arr.length, &arr.stride, (void**)&arr.data);                     \
        return arr;                                                                                          \
    }                                                                                                        \
                                                                                                             \
    BINLINE void name##_array_destroy(name##_array* arr)                                                     \
    {                                                                                                        \
        _barray_free(&arr->length, &arr->stride, (void**)&arr->data);                                        \
    }                                                                                                        \
                                                                                                             \
    BINLINE name##_array_it name##_array_iterator_begin(name##_array* arr)                                   \
    {                                                                                                        \
        name##_array_it it;                                                                                  \
        it.arr = arr;                                                                                        \
        it.pos = 0;                                                                                          \
        return it;                                                                                           \
    }                                                                                                        \
                                                                                                             \
    BINLINE b8 name##_array_iterator_end(const name##_array_it* it) { return it->pos >= it->arr->length; }   \
    BINLINE type* name##_array_iterator_value(const name##_array_it* it) { return &it->arr->data[it->pos]; } \
    BINLINE void name##_array_iterator_next(name##_array_it* it) { it->pos++; }                              \
    BINLINE void name##_array_iterator_prev(name##_array_it* it) { it->pos--; }

// Create an array type of the given type. For advanced types or pointers, use ARRAY_TYPE_NAMED directly
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
