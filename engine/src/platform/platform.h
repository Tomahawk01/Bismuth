#pragma once

#include "defines.h"

typedef struct platform_system_config
{
    const char* application_name;
    // Initial x position of the main window
    i32 x;
    // Initial y position of the main window
    i32 y;
    // Initial width of the main window
    i32 width;
    // Initial height of the main window
    i32 height;
} platform_system_config;

b8 platform_system_startup(u64* memory_requirement, void* state, void* config);

void platform_system_shutdown(void* plat_state);

b8 platform_pump_messages();

void* platform_allocate(u64 size, b8 aligned);
void platform_free(void* block, b8 aligned);
void* platform_zero_memory(void* block, u64 size);
void* platform_copy_memory(void* dest, const void* source, u64 size);
void* platform_set_memory(void* dest, i32 value, u64 size);

void platform_console_write(const char* message, u8 color);
void platform_console_write_error(const char* message, u8 color);

f64 platform_get_absolute_time();

// Sleep on the thread for the provided ms. This blocks the main thread.
void platform_sleep(u64 ms);

i32 platform_get_processor_count();
