#pragma once

#include "defines.h"
#include "memory/linear_allocator.h"

struct frame_data;

typedef b8 (*PFN_system_initialize)(u64* memory_requirement, void* memory, void* config);
typedef void (*PFN_system_shutdown)(void* state);
typedef b8 (*PFN_system_update)(void* state, struct frame_data* p_frame_data);

typedef struct b_system
{
    u64 state_size;
    void* state;
    PFN_system_initialize initialize;
    PFN_system_shutdown shutdown;
    // Function pointer for the system's update routine, called every frame. Optional
    PFN_system_update update;
} b_system;

#define B_SYSTEM_TYPE_MAX_COUNT 512

typedef enum b_system_type
{
    B_SYSTEM_TYPE_MEMORY = 0,
    B_SYSTEM_TYPE_CONSOLE,
    B_SYSTEM_TYPE_BVAR,
    B_SYSTEM_TYPE_EVENT,
    B_SYSTEM_TYPE_LOGGING,
    B_SYSTEM_TYPE_INPUT,
    B_SYSTEM_TYPE_PLATFORM,
    B_SYSTEM_TYPE_RESOURCE,
    B_SYSTEM_TYPE_SHADER,
    B_SYSTEM_TYPE_JOB,
    B_SYSTEM_TYPE_TEXTURE,
    B_SYSTEM_TYPE_FONT,
    B_SYSTEM_TYPE_CAMERA,
    B_SYSTEM_TYPE_RENDERER,
    B_SYSTEM_TYPE_RENDER_VIEW,
    B_SYSTEM_TYPE_MATERIAL,
    B_SYSTEM_TYPE_GEOMETRY,
    B_SYSTEM_TYPE_LIGHT,
    B_SYSTEM_TYPE_AUDIO,

    // NOTE: Anything between 127-254 is extension space
    B_SYSTEM_TYPE_KNOWN_MAX = 127,

    // NOTE: Anything beyond this is in user space
    B_SYSTEM_TYPE_EXT_MAX  = 255,

    // User-space max
    B_SYSTEM_TYPE_USER_MAX = B_SYSTEM_TYPE_MAX_COUNT,
    // Max, including all user-space types
    B_SYSTEM_TYPE_MAX = B_SYSTEM_TYPE_USER_MAX
} b_system_type;

typedef struct systems_manager_state
{
    linear_allocator systems_allocator;
    b_system systems[B_SYSTEM_TYPE_MAX_COUNT];
} systems_manager_state;

struct application_config;

b8 systems_manager_initialize(systems_manager_state* state, struct application_config* app_config);
b8 systems_manager_post_boot_initialize(systems_manager_state* state, struct application_config* app_config);
void systems_manager_shutdown(systems_manager_state* state);
b8 systems_manager_update(systems_manager_state* state, struct frame_data* p_frame_data);

BAPI b8 systems_manager_register(
    systems_manager_state* state,
    u16 type,
    PFN_system_initialize initialize,
    PFN_system_shutdown shutdown,
    PFN_system_update update,
    void* config);

BAPI void* systems_manager_get_state(u16 type);
