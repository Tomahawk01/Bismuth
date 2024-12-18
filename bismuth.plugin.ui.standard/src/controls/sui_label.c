#include "sui_label.h"

#include "standard_ui_defines.h"

#include <containers/darray.h>
#include <debug/bassert.h>
#include <defines.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <renderer/renderer_frontend.h>
#include <resources/resource_types.h>
#include <strings/bstring.h>
#include <systems/font_system.h>
#include <systems/shader_system.h>

typedef struct sui_label_pending_data
{
    u32 quad_count;
    u64 vertex_buffer_size;
    u64 vertex_buffer_offset;
    u64 index_buffer_size;
    u64 index_buffer_offset;
    vertex_2d* vertex_buffer_data;
    u32* index_buffer_data;
} sui_label_pending_data;

static void sui_label_control_render_frame_prepare(standard_ui_state* state, struct sui_control* self, const struct frame_data* p_frame_data);

b8 sui_label_control_create(standard_ui_state* state, const char* name, font_type type, const char* font_name, u16 font_size, const char* text, struct sui_control* out_control)
{
    if (!sui_base_control_create(state, name, out_control))
        return false;

    out_control->internal_data_size = sizeof(sui_label_internal_data);
    out_control->internal_data = ballocate(out_control->internal_data_size, MEMORY_TAG_UI);
    sui_label_internal_data* typed_data = out_control->internal_data;

    typed_data->color = vec4_one();

    // Assign function pointers
    out_control->destroy = sui_label_control_destroy;
    out_control->load = sui_label_control_load;
    out_control->unload = sui_label_control_unload;
    out_control->update = sui_label_control_update;
    out_control->render_prepare = sui_label_control_render_frame_prepare;
    out_control->render = sui_label_control_render;

    out_control->name = string_duplicate(name);

    // Assign the type first
    typed_data->type = type;

    // Acquire font of the correct type and assign its internal data.
    // This also gets atlas texture
    typed_data->data = font_system_acquire(font_name, font_size, typed_data->type);
    if (!typed_data->data) {
        BERROR("Unable to acquire font: '%s'. ui_text cannot be created", font_name);
        return false;
    }

    typed_data->vertex_buffer_offset = INVALID_ID_U64;
    typed_data->vertex_buffer_size = INVALID_ID_U64;
    typed_data->index_buffer_offset = INVALID_ID_U64;
    typed_data->index_buffer_size = INVALID_ID_U64;
    // Default quad count is 0 until the first geometry regeneration happens
    typed_data->quad_count = 0;

    // Set text if applicable
    if (text && string_length(text) > 0)
        sui_label_text_set(state, out_control, text);
    else
        sui_label_text_set(state, out_control, "");

    bhandle sui_shader = shader_system_get(bname_create(STANDARD_UI_SHADER_NAME));
    // Acquire group resources for this control
    if (!shader_system_shader_group_acquire(sui_shader, &typed_data->group_id))
    {
        BFATAL("Unable to acquire shader group resources for button");
        return false;
    }
    typed_data->group_generation = INVALID_ID_U16;

    // Also acquire per-draw resources
    if (!shader_system_shader_per_draw_acquire(sui_shader, &typed_data->draw_id))
    {
        BFATAL("Unable to acquire shader per-draw resources for button");
        return false;
    }

    typed_data->draw_generation = INVALID_ID_U16;

    if (!font_system_verify_atlas(typed_data->data, text))
    {
        BERROR("Font atlas verification failed");
        return false;
    }

    return true;
}

void sui_label_control_destroy(standard_ui_state* state, struct sui_control* self)
{
    sui_base_control_destroy(state, self);
}

b8 sui_label_control_load(standard_ui_state* state, struct sui_control* self)
{
    if (!sui_base_control_load(state, self))
        return false;

    sui_label_internal_data* typed_data = self->internal_data;

    if (typed_data->text && typed_data->text[0] != 0)
    {
        // Flag it as dirty to ensure it gets updated on the next frame
        typed_data->is_dirty = true;
    }

    return true;
}

void sui_label_control_unload(standard_ui_state* state, struct sui_control* self)
{
    sui_label_internal_data* typed_data = self->internal_data;

    if (typed_data->text)
    {
        u32 text_length = string_length(typed_data->text);
        bfree(typed_data->text, sizeof(char) * text_length + 1, MEMORY_TAG_STRING);
        typed_data->text = 0;
    }

    // Free from vertex buffer
    renderbuffer* vertex_buffer = renderer_renderbuffer_get(RENDERBUFFER_TYPE_VERTEX);
    if (typed_data->vertex_buffer_offset != INVALID_ID_U64)
    {
        if (typed_data->max_text_length > 0)
            renderer_renderbuffer_free(vertex_buffer, sizeof(vertex_2d) * 4 * typed_data->max_quad_count, typed_data->vertex_buffer_offset);
        typed_data->vertex_buffer_offset = INVALID_ID_U64;
    }

    // Free from index buffer
    if (typed_data->index_buffer_offset != INVALID_ID_U64)
    {
        static const u64 quad_index_size = (sizeof(u32) * 6);
        renderbuffer* index_buffer = renderer_renderbuffer_get(RENDERBUFFER_TYPE_INDEX);
        if (typed_data->max_text_length > 0 || typed_data->index_buffer_offset != INVALID_ID_U64)
            renderer_renderbuffer_free(index_buffer, quad_index_size * typed_data->max_quad_count, typed_data->index_buffer_offset);
        typed_data->index_buffer_offset = INVALID_ID_U64;
    }

    // Release group/draw resources
    bhandle sui_shader = shader_system_get(bname_create(STANDARD_UI_SHADER_NAME));
    if (!shader_system_shader_group_release(sui_shader, typed_data->group_id))
        BFATAL("Unable to release group shader resources");
    typed_data->group_id = INVALID_ID;
    if (!shader_system_shader_per_draw_release(sui_shader, typed_data->draw_id))
        BFATAL("Unable to release group shader resources");
    typed_data->draw_id = INVALID_ID;
}

b8 sui_label_control_update(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data)
{
    if (!sui_base_control_update(state, self, p_frame_data))
        return false;

    // ...

    return true;
}

b8 sui_label_control_render(standard_ui_state* state, struct sui_control* self, struct frame_data* p_frame_data, standard_ui_render_data* render_data)
{
    if (!sui_base_control_render(state, self, p_frame_data, render_data))
        return false;

    sui_label_internal_data* typed_data = self->internal_data;

    if (typed_data->quad_count && typed_data->vertex_buffer_offset != INVALID_ID_U64)
    {
        standard_ui_renderable renderable = {0};
        renderable.render_data.unique_id = self->id.uniqueid;
        renderable.render_data.vertex_count = typed_data->quad_count * 4;
        renderable.render_data.vertex_buffer_offset = typed_data->vertex_buffer_offset;
        renderable.render_data.vertex_element_size = sizeof(vertex_2d);
        renderable.render_data.index_count = typed_data->quad_count * 6;
        renderable.render_data.index_buffer_offset = typed_data->index_buffer_offset;
        renderable.render_data.index_element_size = sizeof(u32);

        // FIXME: For some reason, this isn't assigned correctly in some cases for
        // system fonts. Doing this assignment fixes it.
        // typed_data->data->atlas.texture = typed_data->data->atlas_texture;

        // NOTE: Override default UI atlas and use that of the loaded font instead
        // TODO: At this point, should have a separate font shader anyway, since the future will require things like SDF
        renderable.atlas_override = &typed_data->data->atlas_texture;

        renderable.render_data.model = xform_world_get(self->xform);
        renderable.render_data.diffuse_color = typed_data->color;

        renderable.group_id = &typed_data->group_id;
        renderable.group_generation = &typed_data->group_generation;
        renderable.per_draw_id = &typed_data->draw_id;
        renderable.per_draw_generation = &typed_data->draw_generation;

        darray_push(render_data->renderables, renderable);
    }

    return true;
}

void sui_label_text_set(standard_ui_state* state, struct sui_control* self, const char* text)
{
    if (self)
    {
        sui_label_internal_data* typed_data = self->internal_data;

        // If strings are already equal, don't do anything
        if (typed_data->text && strings_equal(text, typed_data->text))
            return;

        if (typed_data->text)
        {
            u32 text_length = string_length(typed_data->text);
            bfree(typed_data->text, sizeof(char) * text_length + 1, MEMORY_TAG_STRING);
        }

        typed_data->text = string_duplicate(text);

        // NOTE: Only bother with verification and setting the dirty flag for non-empty strings
        typed_data->is_dirty = true; // string_length(typed_data->text) > 0;
    }
}

const char* sui_label_text_get(standard_ui_state* state, struct sui_control* self)
{
    if (self && self->internal_data)
    {
        sui_label_internal_data* typed_data = self->internal_data;
        return typed_data->text;
    }

    return 0;
}

void sui_label_color_set(standard_ui_state* state, struct sui_control* self, vec4 color)
{
    if (self && self->internal_data)
    {
        sui_label_internal_data* typed_data = self->internal_data;
        typed_data->color = color;
    }
}

static font_glyph* glyph_from_codepoint(font_data* font, i32 codepoint)
{
    for (u32 i = 0; i < font->glyph_count; ++i)
    {
        if (font->glyphs[i].codepoint == codepoint)
            return &font->glyphs[i];
    }

    BERROR("Unable to find font glyph for codepoint: %s", codepoint);
    return 0;
}

static font_kerning* kerning_from_codepoints(font_data* font, i32 codepoint_0, i32 codepoint_1)
{
    for (u32 i = 0; i < font->kerning_count; ++i)
    {
        font_kerning* k = &font->kernings[i];
        if (k->codepoint_0 == codepoint_0 && k->codepoint_1 == codepoint_1)
            return k;
    }

    return 0;
}

static b8 regenerate_label_geometry(const sui_control* self, sui_label_pending_data* pending_data)
{
    sui_label_internal_data* typed_data = self->internal_data;

    // Get UTF-8 string length
    u32 text_length_utf8 = string_utf8_length(typed_data->text);
    u32 char_length = string_length(typed_data->text);

    // Iterate the string once and count how many quads are required.
    // This allows characters which don't require rendering (spaces, tabs, etc.) to be skipped
    pending_data->quad_count = 0;

    // If text is empty, resetting quad count is enough
    if (text_length_utf8 < 1)
        return true;
    i32* codepoints = ballocate(sizeof(i32) * text_length_utf8, MEMORY_TAG_ARRAY);
    for (u32 c = 0, cp_idx = 0; c < char_length;)
    {
        i32 codepoint = typed_data->text[c];
        u8 advance = 1;

        // Ensure the propert UTF-8 codepoint is being used.
        if (!bytes_to_codepoint(typed_data->text, c, &codepoint, &advance))
        {
            BWARN("Invalid UTF-8 found in string, using unknown codepoint of -1");
            codepoint = -1;
        }

        // Whitespace codepoints do not need to be included in the quad count
        if (!codepoint_is_whitespace(codepoint))
            pending_data->quad_count++;

        c += advance;

        // Add to the codepoint list
        codepoints[cp_idx] = codepoint;
        cp_idx++;
    }

    // Calculate buffer sizes
    static const u64 verts_per_quad = 4;
    static const u8 indices_per_quad = 6;
    
    // Save the data off to a pending structure
    pending_data->vertex_buffer_size = sizeof(vertex_2d) * verts_per_quad * pending_data->quad_count;
    pending_data->index_buffer_size = sizeof(u32) * indices_per_quad * pending_data->quad_count;
    // Temp arrays to hold vertex/index data
    pending_data->vertex_buffer_data = ballocate(pending_data->vertex_buffer_size, MEMORY_TAG_ARRAY);
    pending_data->index_buffer_data = ballocate(pending_data->index_buffer_size, MEMORY_TAG_ARRAY);

    // Generate new geometry for each character
    f32 x = 0;
    f32 y = 0;

    // Iterate the codepoints list
    for (u32 c = 0, q_idx = 0; c < text_length_utf8; ++c)
    {
        i32 codepoint = codepoints[c];

        // Whitespace doesn't get a quad created for it
        if (codepoint == '\n')
        {
            x = 0;
            y += typed_data->data->line_height;
            continue;
        }
        else if (codepoint == '\t')
        {
            // Manually move over by the configured tab advance amount
            x += typed_data->data->tab_x_advance;
            continue;
        }

        // Obtain the glyph
        font_glyph* g = glyph_from_codepoint(typed_data->data, codepoint);
        if (!g)
        {
            BERROR("Unable to find unknown codepoint. Using '?' instead");
            g = glyph_from_codepoint(typed_data->data, '?');
        }

        // If not on the last codepoint, try to find kerning between this and the next codepoint
        i32 kerning_amount = 0;
        if (c < text_length_utf8 - 1)
        {
            i32 next_codepoint = codepoints[c + 1];
            // Try to find kerning
            font_kerning* kerning = kerning_from_codepoints(typed_data->data, codepoint, next_codepoint);
            if (kerning)
                kerning_amount = kerning->amount;
        }

        // Only generate a quad for non-whitespace characters
        if (!codepoint_is_whitespace(codepoint))
        {
            // Generate points for the quad
            f32 minx = x + g->x_offset;
            f32 miny = y + g->y_offset;
            f32 maxx = minx + g->width;
            f32 maxy = miny + g->height;
            f32 tminx = (f32)g->x / typed_data->data->atlas_size_x;
            f32 tmaxx = (f32)(g->x + g->width) / typed_data->data->atlas_size_x;
            f32 tminy = (f32)g->y / typed_data->data->atlas_size_y;
            f32 tmaxy = (f32)(g->y + g->height) / typed_data->data->atlas_size_y;
            // Flip the y axis for system text
            if (typed_data->type == FONT_TYPE_SYSTEM)
            {
                tminy = 1.0f - tminy;
                tmaxy = 1.0f - tmaxy;
            }

            vertex_2d p0 = (vertex_2d){vec2_create(minx, miny), vec2_create(tminx, tminy)};
            vertex_2d p1 = (vertex_2d){vec2_create(maxx, miny), vec2_create(tmaxx, tminy)};
            vertex_2d p2 = (vertex_2d){vec2_create(maxx, maxy), vec2_create(tmaxx, tmaxy)};
            vertex_2d p3 = (vertex_2d){vec2_create(minx, maxy), vec2_create(tminx, tmaxy)};

            // Vertex data
            pending_data->vertex_buffer_data[(q_idx * 4) + 0] = p0; // 0    3
            pending_data->vertex_buffer_data[(q_idx * 4) + 1] = p2; //
            pending_data->vertex_buffer_data[(q_idx * 4) + 2] = p3; //
            pending_data->vertex_buffer_data[(q_idx * 4) + 3] = p1; // 2    1

            // Index data 210301
            pending_data->index_buffer_data[(q_idx * 6) + 0] = (q_idx * 4) + 2;
            pending_data->index_buffer_data[(q_idx * 6) + 1] = (q_idx * 4) + 1;
            pending_data->index_buffer_data[(q_idx * 6) + 2] = (q_idx * 4) + 0;
            pending_data->index_buffer_data[(q_idx * 6) + 3] = (q_idx * 4) + 3;
            pending_data->index_buffer_data[(q_idx * 6) + 4] = (q_idx * 4) + 0;
            pending_data->index_buffer_data[(q_idx * 6) + 5] = (q_idx * 4) + 1;

            // Increment quad index
            q_idx++;
        }

        // Advance by the glyph's advance and kerning
        x += g->x_advance + kerning_amount;
    }

    // Clean up
    if (codepoints)
        bfree(codepoints, sizeof(i32) * text_length_utf8, MEMORY_TAG_ARRAY);

    return true;
}

static void sui_label_control_render_frame_prepare(standard_ui_state* state, struct sui_control* self, const struct frame_data* p_frame_data)
{
    if (self)
    {
        sui_label_internal_data* typed_data = self->internal_data;
        if (typed_data->is_dirty)
        {
            // Verify atlas has the glyphs needed
            if (!font_system_verify_atlas(typed_data->data, typed_data->text))
            {
                BERROR("Font atlas verification failed");
                typed_data->quad_count = 0; // Keep it from drawing
                goto sui_label_frame_prepare_cleanup;
            }

            sui_label_pending_data pending_data = {0};
            if (!regenerate_label_geometry(self, &pending_data))
            {
                BERROR("Error regenerating label geometry");
                typed_data->quad_count = 0; // Keep it from drawing
                goto sui_label_frame_prepare_cleanup;
            }

            renderbuffer* vertex_buffer = renderer_renderbuffer_get(RENDERBUFFER_TYPE_VERTEX);
            renderbuffer* index_buffer = renderer_renderbuffer_get(RENDERBUFFER_TYPE_INDEX);

            u64 old_vertex_size = typed_data->vertex_buffer_size;
            u64 old_vertex_offset = typed_data->vertex_buffer_offset;
            u64 old_index_size = typed_data->index_buffer_size;
            u64 old_index_offset = typed_data->index_buffer_offset;

            // Use the new offsets unless a realloc is needed
            u64 new_vertex_size = pending_data.vertex_buffer_size;
            u64 new_vertex_offset = old_vertex_offset;
            u64 new_index_size = pending_data.index_buffer_size;
            u64 new_index_offset = old_index_offset;

            // A reallocation is required if the text is longer than it previously was
            b8 needs_realloc = pending_data.quad_count > typed_data->max_quad_count;
            if (needs_realloc)
            {
                if (!renderer_renderbuffer_allocate(vertex_buffer, new_vertex_size, &pending_data.vertex_buffer_offset))
                {
                    BERROR("sui_label_control_render_frame_prepare failed to allocate from the renderer's vertex buffer: size=%u, offset=%u", new_vertex_size, pending_data.vertex_buffer_offset);
                    typed_data->quad_count = 0; // Keep it from drawing
                    goto sui_label_frame_prepare_cleanup;
                }
                new_vertex_offset = pending_data.vertex_buffer_offset;

                if (!renderer_renderbuffer_allocate(index_buffer, new_index_size, &pending_data.index_buffer_offset))
                {
                    BERROR("sui_label_control_render_frame_prepare failed to allocate from the renderer's index buffer: size=%u, offset=%u", new_index_size, pending_data.index_buffer_offset);
                    typed_data->quad_count = 0; // Keep it from drawing
                    goto sui_label_frame_prepare_cleanup;
                }
                new_index_offset = pending_data.index_buffer_offset;
            }

            // Load up the data, if there is data to load
            if (pending_data.vertex_buffer_data)
            {
                if (!renderer_renderbuffer_load_range(vertex_buffer, new_vertex_offset, new_vertex_size, pending_data.vertex_buffer_data, true))
                    BERROR("sui_label_control_render_frame_prepare failed to load data into vertex buffer range: size=%u, offset=%u", new_vertex_size, new_vertex_offset);
            }

            if (pending_data.index_buffer_data)
            {
                if (!renderer_renderbuffer_load_range(index_buffer, new_index_offset, new_index_size, pending_data.index_buffer_data, true))
                    BERROR("sui_label_control_render_frame_prepare failed to load data into index buffer range: size=%u, offset=%u", new_index_size, new_index_offset);
            }

            if (needs_realloc)
            {
                // Release the old vertex/index data from the buffers and update the sizes/offsets
                if (old_vertex_offset != INVALID_ID_U64 && old_vertex_size != INVALID_ID_U64)
                {
                    if (!renderer_renderbuffer_free(vertex_buffer, old_vertex_size, old_vertex_offset))
                        BERROR("Failed to free from renderer vertex buffer: size=%u, offset=%u", old_vertex_size, old_vertex_offset);
                }
                if (old_index_offset != INVALID_ID_U64 && old_index_size != INVALID_ID_U64)
                {
                    if (!renderer_renderbuffer_free(index_buffer, old_index_size, old_index_offset))
                        BERROR("Failed to free from renderer index buffer: size=%u, offset=%u", old_index_size, old_index_offset);
                }

                typed_data->vertex_buffer_offset = new_vertex_offset;
                typed_data->vertex_buffer_size = new_vertex_size;
                typed_data->index_buffer_offset = new_index_offset;
                typed_data->index_buffer_size = new_index_size;
            }

            typed_data->quad_count = pending_data.quad_count;

            // Update the max length if the string is now longer
            if (pending_data.quad_count > typed_data->max_quad_count)
                typed_data->max_quad_count = pending_data.quad_count;

            // No longer dirty
            typed_data->is_dirty = false;

        sui_label_frame_prepare_cleanup:
            if (pending_data.vertex_buffer_data)
                bfree(pending_data.vertex_buffer_data, pending_data.vertex_buffer_size, MEMORY_TAG_ARRAY);
            if (pending_data.index_buffer_data)
                bfree(pending_data.index_buffer_data, pending_data.index_buffer_size, MEMORY_TAG_ARRAY);
        }
    }
}
