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

typedef struct dynamic_library_function
{
    const char* name;
    void* pfn;
} dynamic_library_function;

typedef struct dynamic_library
{
    const char* name;
    const char* filename;
    u64 internal_data_size;
    void* internal_data;
    u32 watch_id;

    // darray
    dynamic_library_function* functions;
} dynamic_library;

typedef enum platform_error_code
{
    PLATFORM_ERROR_SUCCESS = 0,
    PLATFORM_ERROR_UNKNOWN = 1,
    PLATFORM_ERROR_FILE_NOT_FOUND = 2,
    PLATFORM_ERROR_FILE_LOCKED = 3,
    PLATFORM_ERROR_FILE_EXISTS = 4
} platform_error_code;

b8 platform_system_startup(u64* memory_requirement, void* state, void* config);
void platform_system_shutdown(void* plat_state);

b8 platform_pump_messages(void);

void* platform_allocate(u64 size, b8 aligned);
void platform_free(void* block, b8 aligned);
void* platform_zero_memory(void* block, u64 size);
void* platform_copy_memory(void* dest, const void* source, u64 size);
void* platform_set_memory(void* dest, i32 value, u64 size);

void platform_console_write(const char* message, u8 color);
void platform_console_write_error(const char* message, u8 color);

f64 platform_get_absolute_time(void);

// Sleep on the thread for the provided ms. This blocks the main thread
BAPI void platform_sleep(u64 ms);

i32 platform_get_processor_count(void);

BAPI void platform_get_handle_info(u64* out_size, void* memory);

BAPI f32 platform_device_pixel_ratio(void);

BAPI b8 platform_dynamic_library_load(const char* name, dynamic_library* out_library);
BAPI b8 platform_dynamic_library_unload(dynamic_library* library);
BAPI b8 platform_dynamic_library_load_function(const char* name, dynamic_library* library);

BAPI const char* platform_dynamic_library_extension(void);
BAPI const char* platform_dynamic_library_prefix(void);

BAPI platform_error_code platform_copy_file(const char *source, const char *dest, b8 overwrite_if_exists);
BAPI b8 platform_watch_file(const char* file_path, u32* out_watch_id);
BAPI b8 platform_unwatch_file(u32 watch_id);
