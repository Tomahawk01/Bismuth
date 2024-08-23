#pragma once

#include "defines.h"
#include "input_types.h"
#include "logger.h"

typedef struct platform_system_config
{
    const char* application_name;
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

struct platform_state;

/** @brief A configuration structure used to create new windows */
typedef struct bwindow_config
{
    i32 position_x;
    i32 position_y;
    u32 width;
    u32 height;
    const char* title;
    const char* name;
} bwindow_config;

struct bwindow_platform_state;
struct bwindow_renderer_state;

// Represents a window in the application
typedef struct bwindow
{
    /** @brief The internal name of the window */
    const char* name;
    /** @brief The title of the window */
    const char* title;

    u16 width;
    u16 height;

    /**
     * @brief Represents the pixel density of this window. Should only ever be
     * read from, as the platform layer is responsible for determining this
     */
    f32 device_pixel_ratio;

    b8 resizing;
    u16 frames_since_resize;

    /** @brief Holds platform-specific data */
    struct bwindow_platform_state* platform_state;

    /** @brief Holds renderer-specific data */
    struct bwindow_renderer_state* renderer_state;
} bwindow;

typedef void (*platform_filewatcher_file_deleted_callback)(u32 watcher_id);
typedef void (*platform_filewatcher_file_written_callback)(u32 watcher_id);
typedef void (*platform_window_closed_callback)(const struct bwindow* window);
typedef void (*platform_window_resized_callback)(const struct bwindow* window);
typedef void (*platform_process_key)(keys key, b8 pressed);
typedef void (*platform_process_mouse_button)(mouse_buttons button, b8 pressed);
typedef void (*platform_process_mouse_move)(i16 x, i16 y);
typedef void (*platform_process_mouse_wheel)(i8 z_delta);

BAPI b8 platform_system_startup(u64* memory_requirement, struct platform_state* state, struct platform_system_config* config);
BAPI void platform_system_shutdown(struct platform_state* state);

BAPI b8 platform_window_create(const bwindow_config* config, struct bwindow* window, b8 show_immediately);
BAPI void platform_window_destroy(struct bwindow* window);
BAPI b8 platform_window_show(struct bwindow* window);
BAPI b8 platform_window_hide(struct bwindow* window);
BAPI const char* platform_window_title_get(const struct bwindow* window);
BAPI b8 platform_window_title_set(struct bwindow* window, const char* title);

BAPI b8 platform_pump_messages(void);

BAPI void* platform_allocate(u64 size, b8 aligned);
BAPI void platform_free(void* block, b8 aligned);
BAPI void* platform_zero_memory(void* block, u64 size);
BAPI void* platform_copy_memory(void* dest, const void* source, u64 size);
BAPI void* platform_set_memory(void* dest, i32 value, u64 size);

BAPI void platform_console_write(struct platform_state* platform, log_level level, const char* message);

BAPI f64 platform_get_absolute_time(void);

// Sleep on the thread for the provided ms. This blocks the main thread
BAPI void platform_sleep(u64 ms);

BAPI i32 platform_get_processor_count(void);

BAPI void platform_get_handle_info(u64* out_size, void* memory);

BAPI f32 platform_device_pixel_ratio(const struct bwindow* window);

BAPI b8 platform_dynamic_library_load(const char* name, dynamic_library* out_library);
BAPI b8 platform_dynamic_library_unload(dynamic_library* library);
BAPI void* platform_dynamic_library_load_function(const char* name, dynamic_library* library);

BAPI const char* platform_dynamic_library_extension(void);
BAPI const char* platform_dynamic_library_prefix(void);

BAPI platform_error_code platform_copy_file(const char *source, const char *dest, b8 overwrite_if_exists);
BAPI void platform_register_watcher_deleted_callback(platform_filewatcher_file_deleted_callback callback);
BAPI void platform_register_watcher_written_callback(platform_filewatcher_file_written_callback callback);
BAPI void platform_register_window_closed_callback(platform_window_closed_callback callback);
BAPI void platform_register_window_resized_callback(platform_window_resized_callback callback);
BAPI void platform_register_process_key(platform_process_key callback);
BAPI void platform_register_process_mouse_button_callback(platform_process_mouse_button callback);
BAPI void platform_register_process_mouse_move_callback(platform_process_mouse_move callback);
BAPI void platform_register_process_mouse_wheel_callback(platform_process_mouse_wheel callback);

BAPI b8 platform_watch_file(const char* file_path, u32* out_watch_id);
BAPI b8 platform_unwatch_file(u32 watch_id);
