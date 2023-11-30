#pragma once

#include <math/geometry_utils.h>

#include "../standard_ui_system.h"

typedef struct sui_button_internal_data
{
    vec2i size;
    vec4 color;
    nine_slice nslice;
    u32 instance_id;
    u64 frame_number;
    u8 draw_index;
} sui_button_internal_data;

BAPI b8 sui_button_control_create(const char* name, struct sui_control* out_control);
BAPI void sui_button_control_destroy(struct sui_control* self);
BAPI b8 sui_button_control_height_set(struct sui_control* self, i32 width);

BAPI b8 sui_button_control_load(struct sui_control* self);
BAPI void sui_button_control_unload(struct sui_control* self);

BAPI b8 sui_button_control_update(struct sui_control* self, struct frame_data* p_frame_data);
BAPI b8 sui_button_control_render(struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI void sui_button_on_mouse_out(struct sui_control* self, struct sui_mouse_event event);
BAPI void sui_button_on_mouse_over(struct sui_control* self, struct sui_mouse_event event);
BAPI void sui_button_on_mouse_down(struct sui_control* self, struct sui_mouse_event event);
BAPI void sui_button_on_mouse_up(struct sui_control* self, struct sui_mouse_event event);
