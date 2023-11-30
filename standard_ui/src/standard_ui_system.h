#pragma once

#include <math/math_types.h>

#include "core/identifier.h"
#include "core/input.h"
#include "defines.h"
#include "renderer/renderer_types.h"
#include "resources/resource_types.h"

#define B_SYSTEM_TYPE_STANDARD_UI_EXT 128

struct frame_data;

typedef struct standard_ui_system_config
{
    u64 max_control_count;
} standard_ui_system_config;

typedef struct standard_ui_renderable
{
    u32* instance_id;
    u64* frame_number;
    texture_map* atlas_override;
    u8* draw_index;
    geometry_render_data render_data;
    geometry_render_data* clip_mask_render_data;
} standard_ui_renderable;

typedef struct standard_ui_render_data
{
    texture_map* ui_atlas;
    // darray
    standard_ui_renderable* renderables;
} standard_ui_render_data;

typedef struct sui_mouse_event
{
    buttons mouse_button;
    i16 x;
    i16 y;
} sui_mouse_event;

typedef enum sui_keyboard_event_type
{
    SUI_KEYBOARD_EVENT_TYPE_PRESS,
    SUI_KEYBOARD_EVENT_TYPE_RELEASE,
} sui_keyboard_event_type;

typedef struct sui_keyboard_event
{
    keys key;
    sui_keyboard_event_type type;
} sui_keyboard_event;

typedef struct sui_clip_mask
{
    u32 reference_id;
    transform clip_xform;
    struct geometry* clip_geometry;
    geometry_render_data render_data;
} sui_clip_mask;

typedef struct sui_control
{
    identifier id;
    transform xform;
    char* name;
    // TODO: Convert to flags
    b8 is_active;
    b8 is_visible;
    b8 is_hovered;
    b8 is_pressed;
    rect_2d bounds;

    struct sui_control* parent;
    // darray
    struct sui_control** children;

    void* internal_data;
    u64 internal_data_size;

    void* user_data;
    u64 user_data_size;

    void (*destroy)(struct sui_control* self);
    b8 (*load)(struct sui_control* self);
    void (*unload)(struct sui_control* self);

    b8 (*update)(struct sui_control* self, struct frame_data* p_frame_data);
    b8 (*render)(struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* reneder_data);

    void (*on_click)(struct sui_control* self, struct sui_mouse_event event);
    void (*on_mouse_down)(struct sui_control* self, struct sui_mouse_event event);
    void (*on_mouse_up)(struct sui_control* self, struct sui_mouse_event event);
    void (*on_mouse_over)(struct sui_control* self, struct sui_mouse_event event);
    void (*on_mouse_out)(struct sui_control* self, struct sui_mouse_event event);
    void (*on_mouse_move)(struct sui_control* self, struct sui_mouse_event event);

    void (*internal_click)(struct sui_control* self, struct sui_mouse_event event);
    void (*internal_mouse_over)(struct sui_control* self, struct sui_mouse_event event);
    void (*internal_mouse_out)(struct sui_control* self, struct sui_mouse_event event);
    void (*internal_mouse_down)(struct sui_control* self, struct sui_mouse_event event);
    void (*internal_mouse_up)(struct sui_control* self, struct sui_mouse_event event);
    void (*internal_mouse_move)(struct sui_control* self, struct sui_mouse_event event);

    void (*on_key)(struct sui_control* self, struct sui_keyboard_event event);

} sui_control;

typedef struct standard_ui_state
{
    standard_ui_system_config config;
    u32 total_control_count;
    u32 active_control_count;
    sui_control** active_controls;
    u32 inactive_control_count;
    sui_control** inactive_controls;
    sui_control root;
    texture_map ui_atlas;

    u64 focused_id;

} standard_ui_state;

BAPI b8 standard_ui_system_initialize(u64* memory_requirement, void* state, void* config);
BAPI void standard_ui_system_shutdown(void* state);

BAPI b8 standard_ui_system_update(void* state, struct frame_data* p_frame_data);
BAPI b8 standard_ui_system_render(void* state, sui_control* root, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI b8 standard_ui_system_update_active(void* state, sui_control* control);

BAPI b8 standard_ui_system_register_control(void* state, sui_control* control);

BAPI b8 standard_ui_system_control_add_child(void* state, sui_control* parent, sui_control* child);
BAPI b8 standard_ui_system_control_remove_child(void* state, sui_control* parent, sui_control* child);

BAPI void standard_ui_system_focus_control(void* state, sui_control* control);

BAPI b8 sui_base_control_create(const char* name, struct sui_control* out_control);
BAPI void sui_base_control_destroy(struct sui_control* self);

BAPI b8 sui_base_control_load(struct sui_control* self);
BAPI void sui_base_control_unload(struct sui_control* self);

BAPI b8 sui_base_control_update(struct sui_control* self, struct frame_data* p_frame_data);
BAPI b8 sui_base_control_render(struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI void sui_control_position_set(struct sui_control* self, vec3 position);
BAPI vec3 sui_control_position_get(struct sui_control* self);
