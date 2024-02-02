#pragma once

#include "standard_ui_system.h"

typedef struct sui_label_pending_data
{
    u64 vertex_buffer_size;
    u64 vertex_buffer_offset;
    u64 index_buffer_size;
    u64 index_buffer_offset;
    vertex_2d* vertex_buffer_data;
    u32* index_buffer_data;
    u32 new_utf8_length;
    u32 new_length;
} sui_label_pending_data;

typedef struct sui_label_internal_data
{
    vec2i size;
    vec4 color;
    u32 instance_id;
    u64 frame_number;
    u8 draw_index;

    font_type type;
    struct font_data* data;
    u64 vertex_buffer_offset;
    u64 index_buffer_offset;
    u64 vertex_buffer_size;
    u64 index_buffer_size;
    char* text;
    u32 max_text_length;
    u32 cached_ut8_length;

    b8 is_dirty;
    sui_label_pending_data pending_data;
} sui_label_internal_data;

BAPI b8 sui_label_control_create(const char* name, font_type type, const char* font_name, u16 font_size, const char* text, struct sui_control* out_control);
BAPI void sui_label_control_destroy(struct sui_control* self);
BAPI b8 sui_label_control_load(struct sui_control* self);
BAPI void sui_label_control_unload(struct sui_control* self);
BAPI b8 sui_label_control_update(struct sui_control* self, struct frame_data* p_frame_data);
BAPI b8 sui_label_control_render(struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data);

BAPI void sui_label_text_set(struct sui_control* self, const char* text);

BAPI const char* sui_label_text_get(struct sui_control* self);
