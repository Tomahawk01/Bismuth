#pragma once

#include "../standard_ui_system.h"

typedef struct sui_panel_internal_data
{
    vec4 rect;
    vec4 color;
    geometry* g;
    u32 instance_id;
    u64 frame_number;
    u8 draw_index;
    b8 is_dirty;
} sui_panel_internal_data;

BAPI b8 sui_panel_control_create(const char* name, vec2 size, vec4 color, struct sui_control* out_control);
BAPI void sui_panel_control_destroy(struct sui_control* self);

BAPI b8 sui_panel_control_load(struct sui_control* self);
BAPI void sui_panel_control_unload(struct sui_control* self);

BAPI b8 sui_panel_control_update(struct sui_control* self, struct frame_data* p_frame_data);
BAPI b8 sui_panel_control_render(struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI vec2 sui_panel_size(struct sui_control* self);
BAPI b8 sui_panel_control_resize(struct sui_control* self, vec2 new_size);
