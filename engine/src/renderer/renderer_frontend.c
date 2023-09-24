#include "renderer_frontend.h"
#include "renderer_backend.h"
#include "core/logger.h"
#include "core/bmemory.h"
#include "math/bmath.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"
#include "systems/texture_system.h"
#include "systems/material_system.h"
#include "systems/shader_system.h"
#include "systems/camera_system.h"
#include "systems/render_view_system.h"

// TODO: temporary
#include "core/bstring.h"
#include "core/event.h"
// TODO: end temporary

typedef struct renderer_system_state
{
    renderer_backend backend;
    u32 material_shader_id;
    u32 ui_shader_id;
    u8 window_render_target_count;
    u32 framebuffer_width;
    u32 framebuffer_height;

    renderpass* world_renderpass;
    renderpass* ui_renderpass;
    b8 resizing;
    u8 frames_since_resize;
} renderer_system_state;

static renderer_system_state* state_ptr;

void regenerate_render_targets();

#define CRITICAL_INIT(op, msg) \
    if (!op)                   \
    {                          \
        BERROR(msg);           \
        return false;          \
    }

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name)
{
    *memory_requirement = sizeof(renderer_system_state);
    if (state == 0)
        return true;
    state_ptr = state;

    // Default framebuffer size
    state_ptr->framebuffer_width = 1280;
    state_ptr->framebuffer_height = 720;
    state_ptr->resizing = false;
    state_ptr->frames_since_resize = 0;

    // TODO: make this configurable     V
    renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, &state_ptr->backend);
    state_ptr->backend.frame_number = 0;

    renderer_backend_config renderer_config = {};
    renderer_config.application_name = application_name;
    renderer_config.on_rendertarget_refresh_required = regenerate_render_targets;

    // Renderpasses
    renderer_config.renderpass_count = 2;
    const char* world_renderpass_name = "Renderpass.Builtin.World";
    const char* ui_renderpass_name = "Renderpass.Builtin.UI";
    renderpass_config pass_configs[2];
    pass_configs[0].name = world_renderpass_name;
    pass_configs[0].prev_name = 0;
    pass_configs[0].next_name = ui_renderpass_name;
    pass_configs[0].render_area = (vec4){0, 0, 1280, 720};
    pass_configs[0].clear_color = (vec4){0.0f, 0.0f, 0.2f, 1.0f};
    pass_configs[0].clear_flags = RENDERPASS_CLEAR_COLOR_BUFFER_FLAG | RENDERPASS_CLEAR_DEPTH_BUFFER_FLAG | RENDERPASS_CLEAR_STENCIL_BUFFER_FLAG;

    pass_configs[1].name = ui_renderpass_name;
    pass_configs[1].prev_name = world_renderpass_name;
    pass_configs[1].next_name = 0;
    pass_configs[1].render_area = (vec4){0, 0, 1280, 720};
    pass_configs[1].clear_color = (vec4){0.0f, 0.0f, 0.2f, 1.0f};
    pass_configs[1].clear_flags = RENDERPASS_CLEAR_NONE_FLAG;

    renderer_config.pass_configs = pass_configs;

    // Initialize backend
    CRITICAL_INIT(state_ptr->backend.initialize(&state_ptr->backend, &renderer_config, &state_ptr->window_render_target_count), "Renderer backend failed to initialize. Shutting down...");

    state_ptr->world_renderpass = state_ptr->backend.renderpass_get(world_renderpass_name);
    state_ptr->world_renderpass->render_target_count = state_ptr->window_render_target_count;
    state_ptr->world_renderpass->targets = ballocate(sizeof(render_target) * state_ptr->window_render_target_count, MEMORY_TAG_ARRAY);

    state_ptr->ui_renderpass = state_ptr->backend.renderpass_get(ui_renderpass_name);
    state_ptr->ui_renderpass->render_target_count = state_ptr->window_render_target_count;
    state_ptr->ui_renderpass->targets = ballocate(sizeof(render_target) * state_ptr->window_render_target_count, MEMORY_TAG_ARRAY);

    regenerate_render_targets();

    // Update main/world renderpass dimensions
    state_ptr->world_renderpass->render_area.x = 0;
    state_ptr->world_renderpass->render_area.y = 0;
    state_ptr->world_renderpass->render_area.z = state_ptr->framebuffer_width;
    state_ptr->world_renderpass->render_area.w = state_ptr->framebuffer_height;

    // Update UI renderpass dimensions
    state_ptr->ui_renderpass->render_area.x = 0;
    state_ptr->ui_renderpass->render_area.y = 0;
    state_ptr->ui_renderpass->render_area.z = state_ptr->framebuffer_width;
    state_ptr->ui_renderpass->render_area.w = state_ptr->framebuffer_height;

    // Shaders
    resource config_resource;
    shader_config* config = 0;

    // Builtin material shader
    CRITICAL_INIT(
        resource_system_load(BUILTIN_SHADER_NAME_MATERIAL, RESOURCE_TYPE_SHADER, &config_resource),
        "Failed to load builtin material shader");
    config = (shader_config*)config_resource.data;
    CRITICAL_INIT(shader_system_create(config), "Failed to load builtin material shader");
    resource_system_unload(&config_resource);
    state_ptr->material_shader_id = shader_system_get_id(BUILTIN_SHADER_NAME_MATERIAL);

    // Builtin UI shader
    CRITICAL_INIT(
        resource_system_load(BUILTIN_SHADER_NAME_UI, RESOURCE_TYPE_SHADER, &config_resource),
        "Failed to load builtin UI shader.");
    config = (shader_config*)config_resource.data;
    CRITICAL_INIT(shader_system_create(config), "Failed to load builtin UI shader");
    resource_system_unload(&config_resource);
    state_ptr->ui_shader_id = shader_system_get_id(BUILTIN_SHADER_NAME_UI);

    return true;
}

void renderer_system_shutdown(void* state)
{
    if (state_ptr)
    {
        // Destroy render targets
        for (u8 i = 0; i < state_ptr->window_render_target_count; ++i)
        {
            state_ptr->backend.render_target_destroy(&state_ptr->world_renderpass->targets[i], true);
            state_ptr->backend.render_target_destroy(&state_ptr->ui_renderpass->targets[i], true);
        }

        state_ptr->backend.shutdown(&state_ptr->backend);
    }
    state_ptr = 0;
}

void renderer_on_resized(u16 width, u16 height)
{
    if (state_ptr)
    {
        // Flag as resizing and store the change
        state_ptr->resizing = true;
        state_ptr->framebuffer_width = width;
        state_ptr->framebuffer_height = height;
        // Also reset frame count since last resize operation
        state_ptr->frames_since_resize = 0;
    }
    else
    {
        BWARN("Renderer backend does not exist to accept resize: %i %i", width, height);
    }
}

b8 renderer_draw_frame(render_packet* packet)
{
    state_ptr->backend.frame_number++;

    // Make sure window is not currently being resized by waiting a designated
    // number of frames after the last resize operation before performing backend updates
    if (state_ptr->resizing)
    {
        state_ptr->frames_since_resize++;

        // If required number of frames have passed since resize, go ahead and perform actual updates
        if (state_ptr->frames_since_resize >= 30)
        {
            f32 width = state_ptr->framebuffer_width;
            f32 height = state_ptr->framebuffer_height;
            render_view_system_on_window_resize(width, height);
            state_ptr->backend.resized(&state_ptr->backend, width, height);

            state_ptr->frames_since_resize = 0;
            state_ptr->resizing = false;
        }
        else
        {
            // Skip rendering the frame and try again next time
            return true;
        }
    }

    // If the begin frame returned successfully, mid-frame operations may continue
    if (state_ptr->backend.begin_frame(&state_ptr->backend, packet->delta_time))
    {
        u8 attachment_index = state_ptr->backend.window_attachment_index_get();

        // Render each view
        for (u32 i = 0; i < packet->view_count; ++i)
        {
            if (!render_view_system_on_render(packet->views[i].view, &packet->views[i], state_ptr->backend.frame_number, attachment_index))
            {
                BERROR("Error rendering view index %i", i);
                return false;
            }
        }

        // End frame. If this fails, it is likely unrecoverable
        b8 result = state_ptr->backend.end_frame(&state_ptr->backend, packet->delta_time);

        if (!result)
        {
            BERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }

    return true;
}

void renderer_texture_create(const u8* pixels, struct texture* texture)
{
    state_ptr->backend.texture_create(pixels, texture);
}

void renderer_texture_destroy(struct texture* texture)
{
    state_ptr->backend.texture_destroy(texture);
}

void renderer_texture_create_writeable(texture* t)
{
    state_ptr->backend.texture_create_writeable(t);
}

void renderer_texture_write_data(texture* t, u32 offset, u32 size, const u8* pixels)
{
    state_ptr->backend.texture_write_data(t, offset, size, pixels);
}

void renderer_texture_resize(texture* t, u32 new_width, u32 new_height)
{
    state_ptr->backend.texture_resize(t, new_width, new_height);
}

b8 renderer_create_geometry(geometry* geometry, u32 vertex_size, u32 vertex_count, const void* vertices, u32 index_size, u32 index_count, const void* indices)
{
    return state_ptr->backend.create_geometry(geometry, vertex_size, vertex_count, vertices, index_size, index_count, indices);
}

void renderer_destroy_geometry(geometry* geometry)
{
    state_ptr->backend.destroy_geometry(geometry);
}

void renderer_draw_geometry(geometry_render_data* data)
{
    state_ptr->backend.draw_geometry(data);
}

b8 renderer_renderpass_begin(renderpass* pass, render_target* target)
{
    return state_ptr->backend.renderpass_begin(pass, target);
}

b8 renderer_renderpass_end(renderpass* pass)
{
    return state_ptr->backend.renderpass_end(pass);
}

renderpass* renderer_renderpass_get(const char* name)
{
    return state_ptr->backend.renderpass_get(name);
}

b8 renderer_shader_create(shader* s, renderpass* pass, u8 stage_count, const char** stage_filenames, shader_stage* stages)
{
    return state_ptr->backend.shader_create(s, pass, stage_count, stage_filenames, stages);
}

void renderer_shader_destroy(shader* s)
{
    state_ptr->backend.shader_destroy(s);
}

b8 renderer_shader_initialize(shader* s)
{
    return state_ptr->backend.shader_initialize(s);
}

b8 renderer_shader_use(shader* s)
{
    return state_ptr->backend.shader_use(s);
}

b8 renderer_shader_bind_globals(shader* s)
{
    return state_ptr->backend.shader_bind_globals(s);
}

b8 renderer_shader_bind_instance(shader* s, u32 instance_id)
{
    return state_ptr->backend.shader_bind_instance(s, instance_id);
}

b8 renderer_shader_apply_globals(shader* s)
{
    return state_ptr->backend.shader_apply_globals(s);
}

b8 renderer_shader_apply_instance(shader* s, b8 needs_update)
{
    return state_ptr->backend.shader_apply_instance(s, needs_update);
}

b8 renderer_shader_acquire_instance_resources(shader* s, texture_map** maps, u32* out_instance_id)
{
    return state_ptr->backend.shader_acquire_instance_resources(s, maps, out_instance_id);
}

b8 renderer_shader_release_instance_resources(shader* s, u32 instance_id)
{
    return state_ptr->backend.shader_release_instance_resources(s, instance_id);
}

b8 renderer_set_uniform(shader* s, shader_uniform* uniform, const void* value)
{
    return state_ptr->backend.shader_set_uniform(s, uniform, value);
}

b8 renderer_texture_map_acquire_resources(struct texture_map* map)
{
    return state_ptr->backend.texture_map_acquire_resources(map);
}

void renderer_texture_map_release_resources(struct texture_map* map)
{
    state_ptr->backend.texture_map_release_resources(map);
}

void renderer_render_target_create(u8 attachment_count, texture** attachments, renderpass* pass, u32 width, u32 height, render_target* out_target)
{
    state_ptr->backend.render_target_create(attachment_count, attachments, pass, width, height, out_target);
}

void renderer_render_target_destroy(render_target* target, b8 free_internal_memory)
{
    state_ptr->backend.render_target_destroy(target, free_internal_memory);
}

void renderer_renderpass_create(renderpass* out_renderpass, f32 depth, u32 stencil, b8 has_prev_pass, b8 has_next_pass)
{
    state_ptr->backend.renderpass_create(out_renderpass, depth, stencil, has_prev_pass, has_next_pass);
}

void renderer_renderpass_destroy(renderpass* pass)
{
    state_ptr->backend.renderpass_destroy(pass);
}

void regenerate_render_targets()
{
    // Create render targets for each
    for (u8 i = 0; i < state_ptr->window_render_target_count; ++i)
    {
        // Destroy old first if they exist
        state_ptr->backend.render_target_destroy(&state_ptr->world_renderpass->targets[i], false);
        state_ptr->backend.render_target_destroy(&state_ptr->ui_renderpass->targets[i], false);

        texture* window_target_texture = state_ptr->backend.window_attachment_get(i);
        texture* depth_target_texture = state_ptr->backend.depth_attachment_get();

        // World render targets
        texture* attachments[2] = {window_target_texture, depth_target_texture};
        state_ptr->backend.render_target_create(
            2,
            attachments,
            state_ptr->world_renderpass,
            state_ptr->framebuffer_width,
            state_ptr->framebuffer_height,
            &state_ptr->world_renderpass->targets[i]);

        // UI render targets
        texture* ui_attachments[1] = {window_target_texture};
        state_ptr->backend.render_target_create(
            1,
            ui_attachments,
            state_ptr->ui_renderpass,
            state_ptr->framebuffer_width,
            state_ptr->framebuffer_height,
            &state_ptr->ui_renderpass->targets[i]);
    }
}
