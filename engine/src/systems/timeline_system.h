#pragma once

#include "core/bhandle.h"
#include "defines.h"

typedef struct timeline_system_config
{
    u32 dummy;
} timeline_system_config;

b8 timeline_system_initialize(u64* memory_requirement, void* memory, void* config);
b8 timeline_system_update(void* state, f32 engine_delta_time);
void timeline_system_shutdown(void* state);

BAPI b_handle timeline_system_create(f32 scale);
BAPI void timeline_system_destroy(b_handle timeline);

BAPI f32 timeline_system_scale_get(b_handle timeline);
BAPI void timeline_system_scale_set(b_handle timeline, f32 scale);

// Total time since timeline start
BAPI f32 timeline_system_total_get(b_handle timeline);
// Time in seconds since the last frame
BAPI f32 timeline_system_delta_get(b_handle timeline);

BAPI b_handle timeline_system_get_engine(void);
BAPI b_handle timeline_system_get_game(void);
