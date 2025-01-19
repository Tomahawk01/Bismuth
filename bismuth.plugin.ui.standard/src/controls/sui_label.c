#include "sui_label.h"

#include "standard_ui_defines.h"
#include "standard_ui_system.h"

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

static void sui_label_control_render_frame_prepare(standard_ui_state* state, struct sui_control* self, const struct frame_data* p_frame_data);

b8 sui_label_control_create(standard_ui_state* state, const char* name, font_type type, bname font_name, u16 font_size, const char* text, struct sui_control* out_control)
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
    switch (typed_data->type)
    {
    case FONT_TYPE_BITMAP:
        if (!font_system_bitmap_font_acquire(state->font_system, font_name, &typed_data->bitmap_font))
        {
            BERROR("Failed to acquire bitmap font for sui_label. See logs for details. Creation failed");
            return false;
        }
        break;
    case FONT_TYPE_SYSTEM:
        if (!font_system_system_font_acquire(state->font_system, font_name, font_size, &typed_data->system_font))
        {
            BERROR("Failed to acquire system font variant for sui_label. See logs for details. Creation failed");
            return false;
        }
        break;
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

    bhandle sui_shader = shader_system_get(bname_create(STANDARD_UI_SHADER_NAME), bname_create(PACKAGE_NAME_STANDARD_UI));
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

    if (typed_data->type == FONT_TYPE_SYSTEM)
    {
        // Verify atlas has the glyphs needed
        if (!font_system_system_font_verify_atlas(state->font_system, typed_data->system_font, text))
        {
            BERROR("Font atlas verification failed");
            return false;
        }
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
        string_free(typed_data->text);
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
    bhandle sui_shader = shader_system_get(bname_create(STANDARD_UI_SHADER_NAME), bname_create(PACKAGE_NAME_STANDARD_UI));
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
        if (typed_data->type == FONT_TYPE_BITMAP)
        {
            renderable.atlas_override = font_system_bitmap_font_atlas_get(state->font_system, typed_data->bitmap_font);
        }
        else if (typed_data->type == FONT_TYPE_SYSTEM)
        {
            renderable.atlas_override = font_system_system_font_atlas_get(state->font_system, typed_data->system_font);
        }

        if (!renderable.atlas_override)
        {
            // TODO: bleat
        }

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
            string_free(typed_data->text);
            typed_data->text = 0;
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

f32 sui_label_line_height_get(standard_ui_state* state, struct sui_control* self)
{
    if (self && self->internal_data)
    {
        sui_label_internal_data* typed_data = self->internal_data;
        if (typed_data->type == FONT_TYPE_BITMAP)
        {
            return font_system_bitmap_font_line_height_get(state->font_system, typed_data->bitmap_font);
        }
        else
        {
            return font_system_system_font_line_height_get(state->font_system, typed_data->system_font);
        }
    }

    return 0;
}

static b8 regenerate_label_geometry(standard_ui_state* state, const sui_control* self, font_geometry* pending_data)
{
    sui_label_internal_data* typed_data = self->internal_data;

    if (typed_data->type == FONT_TYPE_BITMAP)
    {
        return font_system_bitmap_font_generate_geometry(state->font_system, typed_data->bitmap_font, typed_data->text, pending_data);
    }
    else if (typed_data->type == FONT_TYPE_SYSTEM)
    {
        return font_system_system_font_generate_geometry(state->font_system, typed_data->system_font, typed_data->text, pending_data);
    }

    return false;
}

static void sui_label_control_render_frame_prepare(standard_ui_state* state, struct sui_control* self, const struct frame_data* p_frame_data)
{
    if (self)
    {
        sui_label_internal_data* typed_data = self->internal_data;
        if (typed_data->is_dirty)
        {
            if (typed_data->type == FONT_TYPE_SYSTEM)
            {
                // Verify atlas has the glyphs needed
                if (!font_system_system_font_verify_atlas(state->font_system, typed_data->system_font, typed_data->text))
                {
                    BERROR("Font atlas verification failed");
                    typed_data->quad_count = 0; // Keep it from drawing
                    goto sui_label_frame_prepare_cleanup;
                }
            }

            font_geometry new_geometry = {0};
            if (!regenerate_label_geometry(state, self, &new_geometry))
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
            u64 new_vertex_size = new_geometry.vertex_buffer_size;
            u64 new_vertex_offset = old_vertex_offset;
            u64 new_index_size = new_geometry.index_buffer_size;
            u64 new_index_offset = old_index_offset;

            // A reallocation is required if the text is longer than it previously was
            b8 needs_realloc = new_geometry.quad_count > typed_data->max_quad_count;
            if (needs_realloc)
            {
                if (!renderer_renderbuffer_allocate(vertex_buffer, new_vertex_size, &new_vertex_offset))
                {
                    BERROR("sui_label_control_render_frame_prepare failed to allocate from the renderer's vertex buffer: size=%u, offset=%u", new_vertex_size, new_vertex_offset);
                    typed_data->quad_count = 0; // Keep it from drawing
                    goto sui_label_frame_prepare_cleanup;
                }

                if (!renderer_renderbuffer_allocate(index_buffer, new_index_size, &new_index_offset))
                {
                    BERROR("sui_label_control_render_frame_prepare failed to allocate from the renderer's index buffer: size=%u, offset=%u", new_index_size, new_index_offset);
                    typed_data->quad_count = 0; // Keep it from drawing
                    goto sui_label_frame_prepare_cleanup;
                }
            }

            // Load up the data, if there is data to load
            if (new_geometry.vertex_buffer_data)
            {
                if (!renderer_renderbuffer_load_range(vertex_buffer, new_vertex_offset, new_vertex_size, new_geometry.vertex_buffer_data, true))
                    BERROR("sui_label_control_render_frame_prepare failed to load data into vertex buffer range: size=%u, offset=%u", new_vertex_size, new_vertex_offset);
            }

            if (new_geometry.index_buffer_data)
            {
                if (!renderer_renderbuffer_load_range(index_buffer, new_index_offset, new_index_size, new_geometry.index_buffer_data, true))
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

            typed_data->quad_count = new_geometry.quad_count;

            // Update the max length if the string is now longer
            if (new_geometry.quad_count > typed_data->max_quad_count)
                typed_data->max_quad_count = new_geometry.quad_count;

            // No longer dirty
            typed_data->is_dirty = false;

        sui_label_frame_prepare_cleanup:
            if (new_geometry.vertex_buffer_data)
                bfree(new_geometry.vertex_buffer_data, new_geometry.vertex_buffer_size, MEMORY_TAG_ARRAY);
            if (new_geometry.index_buffer_data)
                bfree(new_geometry.index_buffer_data, new_geometry.index_buffer_size, MEMORY_TAG_ARRAY);
        }
    }
}
