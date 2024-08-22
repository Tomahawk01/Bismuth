#pragma once

#include "../standard_ui_system.h"

#include "math/nine_slice.h"

typedef struct sui_button_internal_data
{
    vec2i size;
    vec4 color;
    nine_slice nslice;
    u32 instance_id;
    u64 frame_number;
    u8 draw_index;
} sui_button_internal_data;

BAPI b8 sui_button_control_create(standard_ui_state* state, const char* name, struct sui_control* out_control);
BAPI void sui_button_control_destroy(standard_ui_state* state, struct sui_control* self);
BAPI b8 sui_button_control_height_set(standard_ui_state* state, struct sui_control* self, i32 width);

BAPI b8 sui_button_control_load(standard_ui_state* state, struct sui_control* self);
BAPI void sui_button_control_unload(standard_ui_state* state, struct sui_control* self);

BAPI b8 sui_button_control_update(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data);
BAPI b8 sui_button_control_render(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI void sui_button_on_mouse_out(standard_ui_state* state, struct sui_control* self, struct sui_mouse_event event);
BAPI void sui_button_on_mouse_over(standard_ui_state* state, struct sui_control* self, struct sui_mouse_event event);
BAPI void sui_button_on_mouse_down(standard_ui_state* state, struct sui_control* self, struct sui_mouse_event event);
BAPI void sui_button_on_mouse_up(standard_ui_state* state, struct sui_control* self, struct sui_mouse_event event);
