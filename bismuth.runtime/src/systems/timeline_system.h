#pragma once

#include "identifiers/bhandle.h"
#include "defines.h"

typedef struct timeline_system_config
{
    u32 dummy;
} timeline_system_config;

b8 timeline_system_initialize(u64* memory_requirement, void* memory, void* config);
b8 timeline_system_update(void* state, f32 engine_delta_time);
void timeline_system_shutdown(void* state);

BAPI bhandle timeline_system_create(f32 scale);
BAPI void timeline_system_destroy(bhandle timeline);

BAPI f32 timeline_system_scale_get(bhandle timeline);
BAPI void timeline_system_scale_set(bhandle timeline, f32 scale);

// Total time since timeline start
BAPI f32 timeline_system_total_get(bhandle timeline);
// Time in seconds since the last frame
BAPI f32 timeline_system_delta_get(bhandle timeline);

BAPI bhandle timeline_system_get_engine(void);
BAPI bhandle timeline_system_get_game(void);
