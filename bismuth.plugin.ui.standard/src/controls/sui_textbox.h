#pragma once

#include "standard_ui_system.h"

#include <defines.h>
#include <renderer/nine_slice.h>
#include <renderer/renderer_types.h>
#include <systems/font_system.h>

typedef struct sui_textbox_internal_data
{
    vec2i size;
    vec4 color;
    nine_slice nslice;
    u32 group_id;
    u16 group_generation;
    u32 draw_id;
    u16 draw_generation;
    sui_control content_label;
    sui_control cursor;
    sui_control highlight_box;
    range32 highlight_range;
    u32 cursor_position;
    f32 text_view_offset;
    sui_clip_mask clip_mask;

    // Cached copy of the internal label's line height (taken in turn from its font)
    f32 label_line_height;

    // HACK: Need to store a pointer to the standard ui state here because the event system can only pass a single pointer, which is already occupied by "self"
    struct standard_ui_state* state;
} sui_textbox_internal_data;

BAPI b8 sui_textbox_control_create(standard_ui_state* state, const char* name, font_type type, bname font_name, u16 font_size, const char* text, struct sui_control* out_control);

BAPI void sui_textbox_control_destroy(standard_ui_state* state, struct sui_control* self);

BAPI b8 sui_textbox_control_size_set(standard_ui_state* state, struct sui_control* self, i32 width, i32 height);
BAPI b8 sui_textbox_control_width_set(standard_ui_state* state, struct sui_control* self, i32 width);
BAPI b8 sui_textbox_control_height_set(standard_ui_state* state, struct sui_control* self, i32 height);

BAPI b8 sui_textbox_control_load(standard_ui_state* state, struct sui_control* self);
BAPI void sui_textbox_control_unload(standard_ui_state* state, struct sui_control* self);

BAPI b8 sui_textbox_control_update(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data);
BAPI b8 sui_textbox_control_render(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI const char* sui_textbox_text_get(standard_ui_state* state, struct sui_control* self);
BAPI void sui_textbox_text_set(standard_ui_state* state, struct sui_control* self, const char* text);
BAPI void sui_textbox_on_mouse_down(standard_ui_state* state, struct sui_control* self, struct sui_mouse_event event);
BAPI void sui_textbox_on_mouse_up(standard_ui_state* state, struct sui_control* self, struct sui_mouse_event event);
