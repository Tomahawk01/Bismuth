#pragma once

#include "defines.h"

struct bvar_state;

typedef enum bvar_types
{
    BVAR_TYPE_INT,
    BVAR_TYPE_FLOAT,
    BVAR_TYPE_STRING
} bvar_types;

typedef union bvar_value
{
    i32 i;
    f32 f;
    char* s;
} bvar_value;

typedef struct bvar_change
{
    const char* name;
    bvar_types old_type;
    bvar_types new_type;
    bvar_value new_value;
} bvar_change;

b8 bvar_system_initialize(u64* memory_requirement, struct bvar_state* memory, void* config);
void bvar_system_shutdown(struct bvar_state* state);

BAPI b8 bvar_i32_get(const char* name, i32* out_value);
BAPI b8 bvar_i32_set(const char* name, const char* desc, i32 value);

BAPI b8 bvar_f32_get(const char* name, f32* out_value);
BAPI b8 bvar_f32_set(const char* name, const char* desc, f32 value);

BAPI const char* bvar_string_get(const char* name);
BAPI b8 bvar_string_set(const char* name, const char* desc, const char* value);
