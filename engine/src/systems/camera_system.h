#pragma once

#include "defines.h"
#include "renderer/camera.h"

typedef struct camera_system_config
{
    u16 max_camera_count;
} camera_system_config;

#define DEFAULT_CAMERA_NAME "default"

b8 camera_system_initialize(u64* memory_requirement, void* state, camera_system_config config);
void camera_system_shutdown(void* state);

camera* camera_system_acquire(const char* name);
void camera_system_release(const char* name);

BAPI camera* camera_system_get_default();
