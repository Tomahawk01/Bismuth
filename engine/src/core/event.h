#pragma once

#include "defines.h"

typedef struct event_context
{
    // 128 bytes
    union
    {
        i64 i64[2];
        u64 u64[2];
        f64 f64[2];

        i32 i32[4];
        u32 u32[4];
        f32 f32[4];

        i16 i16[8];
        u16 u16[8];

        i8 i8[16];
        u8 u8[16];

        char c[16];
    } data;
} event_context;

// Should return true if handled
typedef b8 (*PFN_on_event)(u16 code, void* sender, void* listener_inst, event_context data);

b8 event_system_initialize(u64* memory_requirement, void* state, void* config);
void event_system_shutdown(void* state);

/**
 * Register to listen for when events are sent with the provided code. Events with duplicate
 * listener/callback combos will not be registered again and will cause this to return false.
 * @param code The event code to listen for.
 * @param listener A pointer to a listener instance. Can be 0/NULL.
 * @param on_event The callback function pointer to be invoked when the event code is fired.
 * @returns true if the event is successfully registered; otherwise false.
 */
BAPI b8 event_register(u16 code, void* listener, PFN_on_event on_event);

/**
 * Unregister from listening for when events are sent with the provided code. If no matching
 * registration is found, this function returns false.
 * @param code The event code to stop listening for.
 * @param listener A pointer to a listener instance. Can be 0/NULL.
 * @param on_event The callback function pointer to be unregistered.
 * @returns true if the event is successfully unregistered; otherwise false.
 */
BAPI b8 event_unregister(u16 code, void* listener, PFN_on_event on_event);

/**
 * Fires an event to listeners of the given code. If an event handler returns 
 * true, the event is considered handled and is not passed on to any more listeners.
 * @param code The event code to fire.
 * @param sender A pointer to the sender. Can be 0/NULL.
 * @param data The event data.
 * @returns true if handled, otherwise false.
 */
BAPI b8 event_fire(u16 code, void* sender, event_context context);

// System internal event codes. Application should use codes beyond 255
typedef enum system_event_code
{
    // Shuts the application down on the next frame.
    EVENT_CODE_APPLICATION_QUIT = 0x01,

    // Keyboard key pressed
    EVENT_CODE_KEY_PRESSED = 0x02,

    // Keyboard key released
    EVENT_CODE_KEY_RELEASED = 0x03,

    // Mouse button pressed
    EVENT_CODE_BUTTON_PRESSED = 0x04,

    // Mouse button released
    EVENT_CODE_BUTTON_RELEASED = 0x05,

    // Mouse button pressed then released
    EVENT_CODE_BUTTON_CLICKED = 0x06,

    // Mouse moved
    EVENT_CODE_MOUSE_MOVED = 0x07,

    // Mouse wheel moved
    EVENT_CODE_MOUSE_WHEEL = 0x08,

    // Resize/resolution changed from the OS
    EVENT_CODE_RESIZED = 0x09,

    // Change render mode for debugging purposes
    EVENT_CODE_SET_RENDER_MODE = 0x0A,

    EVENT_CODE_DEBUG0 = 0x10,
    EVENT_CODE_DEBUG1 = 0x11,
    EVENT_CODE_DEBUG2 = 0x12,
    EVENT_CODE_DEBUG3 = 0x13,
    EVENT_CODE_DEBUG4 = 0x14,

    // Hovered-over object id, if there is one
    EVENT_CODE_OBJECT_HOVER_ID_CHANGED = 0x15,

    // An event fired by renderer backend to indicate when any render targets associated with default window resources need to be refreshed
    EVENT_CODE_DEFAULT_RENDERTARGET_REFRESH_REQUIRED = 0x16,

    EVENT_CODE_BVAR_CHANGED = 0x17,

    EVENT_CODE_WATCHED_FILE_WRITTEN = 0x18,
    EVENT_CODE_WATCHED_FILE_DELETED = 0x19,

    EVENT_CODE_MOUSE_DRAGGED = 0x20,
    EVENT_CODE_MOUSE_DRAG_BEGIN = 0x21,
    EVENT_CODE_MOUSE_DRAG_END = 0x22,

    MAX_EVENT_CODE = 0xFF
} system_event_code;
