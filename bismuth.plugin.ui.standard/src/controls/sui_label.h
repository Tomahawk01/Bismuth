#pragma once

#include "standard_ui_system.h"

#include "systems/font_system.h"

typedef struct sui_label_internal_data
{
    vec2i size;
    vec4 color;
    u32 group_id;
    u16 group_generation;
    u32 draw_id;
    u16 draw_generation;

    font_type type;
    // Only used when set to use a bitmap font
    bhandle bitmap_font;
    // Only used when set to use a system font
    system_font_variant system_font;

    u64 vertex_buffer_offset;
    u64 index_buffer_offset;
    u64 vertex_buffer_size;
    u64 index_buffer_size;
    char* text;
    u32 max_text_length;
    u32 quad_count;
    u32 max_quad_count;

    b8 is_dirty;
} sui_label_internal_data;

BAPI b8 sui_label_control_create(standard_ui_state* state, const char* name, font_type type, bname font_name, u16 font_size, const char* text, struct sui_control* out_control);
BAPI void sui_label_control_destroy(standard_ui_state* state, struct sui_control* self);
BAPI b8 sui_label_control_load(standard_ui_state* state, struct sui_control* self);
BAPI void sui_label_control_unload(standard_ui_state* state, struct sui_control* self);
BAPI b8 sui_label_control_update(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data);
BAPI b8 sui_label_control_render(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI void sui_label_text_set(standard_ui_state* state, struct sui_control* self, const char* text);

BAPI const char* sui_label_text_get(standard_ui_state* state, struct sui_control* self);
BAPI void sui_label_color_set(standard_ui_state* state, struct sui_control* self, vec4 color);

BAPI f32 sui_label_line_height_get(standard_ui_state* state, struct sui_control* self);
