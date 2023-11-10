#pragma once

#include "defines.h"

b8 bvar_initialize(u64* memory_requirement, void* memory, void* config);
void bvar_shutdown(void* state);

BAPI b8 bvar_int_create(const char* name, i32 value);
BAPI b8 bvar_int_get(const char* name, i32* out_value);
BAPI b8 bvar_int_set(const char* name, i32 value);
