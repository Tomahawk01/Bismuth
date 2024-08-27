#pragma once

#include "defines.h"

typedef struct stackarray_base
{
    u32 length;
    u32 stride;
    void* p_data;
} stackarray_base;

typedef struct stackarray_iterator
{
    stackarray_base* arr;
    i32 pos;
    i32 dir;
    b8 (*end)(const struct stackarray_iterator* it);
    void* (*value)(const struct stackarray_iterator* it);
    void (*next)(struct stackarray_iterator* it);
    void (*prev)(struct stackarray_iterator* it);
} stackarray_iterator;

BAPI stackarray_iterator stackarray_iterator_begin(stackarray_base* arr);
BAPI stackarray_iterator stackarray_iterator_rbegin(stackarray_base* arr);
BAPI b8 stackarray_iterator_end(const stackarray_iterator* it);
BAPI void* stackarray_iterator_value(const stackarray_iterator* it);
BAPI void stackarray_iterator_next(stackarray_iterator* it);
BAPI void stackarray_iterator_prev(stackarray_iterator* it);

#define STACKARRAY_TYPE_NAMED(type, name, len)                                         \
    typedef struct stackarray_##name##_##len {                                         \
        stackarray_base base;                                                          \
        type data[len];                                                                \
        stackarray_iterator (*begin)(stackarray_base * arr);                           \
        stackarray_iterator (*rbegin)(stackarray_base * arr);                          \
    } stackarray_##name##_##len;                                                       \
                                                                                       \
    BINLINE stackarray_##name##_##len stackarray_##name##_##len##_create(void) {       \
        stackarray_##name##_##len arr;                                                 \
        bzero_memory(&arr, sizeof(stackarray_##name##_##len));                         \
        arr.base.p_data = arr.data;                                                    \
        arr.base.length = len;                                                         \
        arr.base.stride = sizeof(type);                                                \
        arr.begin = stackarray_iterator_begin;                                         \
        arr.rbegin = stackarray_iterator_rbegin;                                       \
        return arr;                                                                    \
    }                                                                                  \
                                                                                       \
    BINLINE void stackarray_##name##_##len##_destroy(stackarray_##name##_##len* arr) { \
        bzero_memory(arr, sizeof(stackarray_##name##_##len));                          \
        arr->begin = 0;                                                                \
        arr->rbegin = 0;                                                               \
    }

// Create an array type of the given type. For advanced types or pointers, use ARRAY_TYPE_NAMED directly
#define STACKARRAY_TYPE(type, length) STACKARRAY_TYPE_NAMED(type, type, length)

// EXAMPLES:
/*
// Creation of stackarray for standard/simple types:
STACKARRAY_TYPE(b8, 4);
STACKARRAY_TYPE(u8, 2);
STACKARRAY_TYPE(u16, 3);
STACKARRAY_TYPE(u32, 4);
STACKARRAY_TYPE(u64, 5);
STACKARRAY_TYPE(i8, 6);
STACKARRAY_TYPE(i16, 7);
STACKARRAY_TYPE(i32, 8);
STACKARRAY_TYPE(i64, 9);
STACKARRAY_TYPE(f32, 10);
STACKARRAY_TYPE(f64, 11);
*/

// Creation of stackarray "advanced" types, such as strings
/* STACKARRAY_TYPE_NAMED(const char*, string, 4);  */
