/*
#include "render_view_pick.h"

#include "logger.h"
#include "memory/bmemory.h"
#include "core/event.h"
#include "strings/bstring.h"
#include "frame_data.h"
#include "core/uuid.h"
#include "math/bmath.h"
#include "containers/darray.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"
#include "systems/camera_system.h"
#include "renderer/renderer_frontend.h"
#include "renderer/viewport.h"
#include "renderer/renderer_types.h"
#include "systems/xform_system.h"

typedef struct render_view_pick_shader_info
{
    shader* s;
    renderpass* pass;
    u16 id_color_location;
    u16 model_location;
    u16 projection_location;
    u16 view_location;
    mat4 view;
} render_view_pick_shader_info;

typedef struct render_view_pick_internal_data
{
    render_view_pick_shader_info ui_shader_info;
    render_view_pick_shader_info world_shader_info;
    render_view_pick_shader_info terrain_shader_info;

    // Used as color attachment for both renderpasses
    texture color_target_attachment_texture;
    texture depth_target_attachment_texture;

    i32 instance_count;
    b8* instance_updated;

    i16 mouse_x, mouse_y;
} render_view_pick_internal_data;

static b8 on_mouse_moved(u16 code, void* sender, void* listener_inst, event_context event_data)
{
    if (code == EVENT_CODE_MOUSE_MOVED)
    {
        render_view* self = (render_view*)listener_inst;
        render_view_pick_internal_data* data = (render_view_pick_internal_data*)self->internal_data;

        // Update position and regenerate projection matrix
        i16 y = event_data.data.i16[1];
        i16 x = event_data.data.i16[0];

        data->mouse_x = x;
        data->mouse_y = y;
    }
    return false; // Allow other handlers to pick up the event
}

static b8 render_view_on_event(u16 code, void* sender, void* listener_inst, event_context context)
{
    render_view* self = (render_view*)listener_inst;
    if (!self)
        return false;

    switch (code)
    {
        case EVENT_CODE_DEFAULT_RENDERTARGET_REFRESH_REQUIRED:
            // render_view_system_render_targets_regenerate(self);
            // This needs to be consumed by other views, so consider it not handled
            return false;
    }

    return false;
}

static void acquire_shader_instances(const struct render_view* self)
{
    render_view_pick_internal_data* data = self->internal_data;

    u32 instance;
    shader_instance_resource_config instance_resource_config = {0};
    instance_resource_config.uniform_config_count = 0;
    instance_resource_config.uniform_configs = 0;
    // UI shader
    if (!renderer_shader_instance_resources_acquire(data->ui_shader_info.s, &instance_resource_config, &instance))
    {
        BFATAL("render_view_pick failed to acquire UI shader resources");
        return;
    }
    // World shader
    if (!renderer_shader_instance_resources_acquire(data->world_shader_info.s, &instance_resource_config, &instance))
    {
        BFATAL("render_view_pick failed to acquire World shader resources");
        return;
    }
    // Terrain shader
    if (!renderer_shader_instance_resources_acquire(data->terrain_shader_info.s, &instance_resource_config, &instance))
    {
        BFATAL("render_view_pick failed to acquire Terrain shader resources");
        return;
    }
    data->instance_count++;
    darray_push(data->instance_updated, false);
}

void release_shader_instances(const struct render_view* self)
{
    render_view_pick_internal_data* data = self->internal_data;

    for (i32 i = 0; i < data->instance_count; ++i)
    {
        // UI shader
        if (!renderer_shader_instance_resources_release(data->ui_shader_info.s, i))
            BWARN("Failed to release UI shader resources");

        // World shader
        if (!renderer_shader_instance_resources_release(data->world_shader_info.s, i))
            BWARN("Failed to release World shader resources");
        
        // Terrain shader
        if (!renderer_shader_instance_resources_release(data->terrain_shader_info.s, i))
            BWARN("Failed to release Terrain shader resources");
    }
    darray_destroy(data->instance_updated);
}

b8 render_view_pick_on_registered(struct render_view* self)
{
    if (self)
    {
        self->internal_data = ballocate(sizeof(render_view_pick_internal_data), MEMORY_TAG_RENDERER);
        render_view_pick_internal_data* data = self->internal_data;

        data->instance_updated = darray_create(b8);

        data->world_shader_info.pass = &self->passes[0];
        data->terrain_shader_info.pass = &self->passes[0];
        data->ui_shader_info.pass = &self->passes[1];

        // Builtin UI Pick shader
        const char* ui_shader_name = "Shader.Builtin.UIPick";
        resource config_resource;
        if (!resource_system_load(ui_shader_name, RESOURCE_TYPE_SHADER, 0, &config_resource))
        {
            BERROR("Failed to load builtin UI Pick shader");
            return false;
        }
        shader_config* config = (shader_config*)config_resource.data;
        if (!shader_system_create(data->ui_shader_info.pass, config))
        {
            BERROR("Failed to load builtin UI Pick shader");
            return false;
        }
        resource_system_unload(&config_resource);
        data->ui_shader_info.s = shader_system_get(ui_shader_name);

        // Extract uniform locations
        data->ui_shader_info.id_color_location = shader_system_uniform_location(data->ui_shader_info.s, "id_color");
        data->ui_shader_info.model_location = shader_system_uniform_location(data->ui_shader_info.s, "model");
        data->ui_shader_info.projection_location = shader_system_uniform_location(data->ui_shader_info.s, "projection");
        data->ui_shader_info.view_location = shader_system_uniform_location(data->ui_shader_info.s, "view");

        // Default UI properties
        data->ui_shader_info.view = mat4_identity();

        // Builtin World Pick shader
        const char* world_shader_name = "Shader.Builtin.WorldPick";
        if (!resource_system_load(world_shader_name, RESOURCE_TYPE_SHADER, 0, &config_resource))
        {
            BERROR("Failed to load builtin World Pick shader");
            return false;
        }
        config = (shader_config*)config_resource.data;
        if (!shader_system_create(data->world_shader_info.pass, config))
        {
            BERROR("Failed to load builtin World Pick shader");
            return false;
        }
        resource_system_unload(&config_resource);
        data->world_shader_info.s = shader_system_get(world_shader_name);

        // Extract uniform locations.
        data->world_shader_info.id_color_location = shader_system_uniform_location(data->world_shader_info.s, "id_color");
        data->world_shader_info.model_location = shader_system_uniform_location(data->world_shader_info.s, "model");
        data->world_shader_info.projection_location = shader_system_uniform_location(data->world_shader_info.s, "projection");
        data->world_shader_info.view_location = shader_system_uniform_location(data->world_shader_info.s, "view");

        // Default World properties
        data->world_shader_info.view = mat4_identity();

        // Builtin Terrain Pick shader
        const char* terrain_shader_name = "Shader.Builtin.TerrainPick";
        if (!resource_system_load(terrain_shader_name, RESOURCE_TYPE_SHADER, 0, &config_resource))
        {
            BERROR("Failed to load builtin Terrain Pick shader");
            return false;
        }
        config = (shader_config*)config_resource.data;
        if (!shader_system_create(data->terrain_shader_info.pass, config))
        {
            BERROR("Failed to load builtin Terrain Pick shader");
            return false;
        }
        resource_system_unload(&config_resource);
        data->terrain_shader_info.s = shader_system_get(terrain_shader_name);

        // Extract uniform locations
        data->terrain_shader_info.id_color_location = shader_system_uniform_location(data->terrain_shader_info.s, "id_color");
        data->terrain_shader_info.model_location = shader_system_uniform_location(data->terrain_shader_info.s, "model");
        data->terrain_shader_info.projection_location = shader_system_uniform_location(data->terrain_shader_info.s, "projection");
        data->terrain_shader_info.view_location = shader_system_uniform_location(data->terrain_shader_info.s, "view");

        // Default terrain properties
        data->terrain_shader_info.view = mat4_identity();

        data->instance_count = 0;

        bzero_memory(&data->color_target_attachment_texture, sizeof(texture));
        bzero_memory(&data->depth_target_attachment_texture, sizeof(texture));

        // Register for mouse move event
        if (!event_register(EVENT_CODE_MOUSE_MOVED, self, on_mouse_moved))
        {
            BERROR("Unable to listen for mouse move event, creation failed");
            return false;
        }

        if (!event_register(EVENT_CODE_DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, render_view_on_event))
        {
            BERROR("Unable to listen for refresh required event, creation failed");
            return false;
        }

        return true;
    }
    BERROR("render_view_pick_on_create - Requires a valid pointer to a view");
    return false;
}

void render_view_pick_on_destroy(struct render_view* self)
{
    if (self && self->internal_data)
    {
        render_view_pick_internal_data* data = self->internal_data;

        // Unregister from events
        event_unregister(EVENT_CODE_MOUSE_MOVED, self, on_mouse_moved);
        event_unregister(EVENT_CODE_DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, render_view_on_event);

        release_shader_instances(self);

        renderer_texture_destroy(&data->color_target_attachment_texture);
        renderer_texture_destroy(&data->depth_target_attachment_texture);

        bfree(self->internal_data, sizeof(render_view_pick_internal_data), MEMORY_TAG_RENDERER);
        self->internal_data = 0;
    }
}

void render_view_pick_on_resize(struct render_view* self, u32 width, u32 height)
{
    // render_view_pick_internal_data* data = self->internal_data;

    self->width = width;
    self->height = height;
}

b8 render_view_pick_on_packet_build(const struct render_view* self, struct frame_data* p_frame_data, struct viewport* v, struct camera* c, void* data, struct render_view_packet* out_packet)
{
    if (!self || !data || !out_packet)
    {
        BWARN("render_view_pick_on_packet_build requires valid pointer to view, packet, and data");
        return false;
    }

    pick_packet_data* packet_data = (pick_packet_data*)data;
    render_view_pick_internal_data* internal_data = (render_view_pick_internal_data*)self->internal_data;

    out_packet->geometries = darray_create(geometry_render_data);
    out_packet->terrain_geometries = darray_create(geometry_render_data);
    out_packet->view = self;
    out_packet->vp = v;

    internal_data->world_shader_info.view = camera_view_get(c);
    internal_data->terrain_shader_info.view = camera_view_get(c);

    // Set pick packet data to extended data
    packet_data->ui_geometry_count = 0;
    out_packet->extended_data = p_frame_data->allocator.allocate(sizeof(pick_packet_data));

    u32 world_geometry_count = !packet_data->world_mesh_data ? 0 : darray_length(packet_data->world_mesh_data);

    u32 highest_instance_id = 0;
    // Iterate all geometries in world data
    for (u32 i = 0; i < world_geometry_count; ++i)
    {
        darray_push(out_packet->geometries, packet_data->world_mesh_data[i]);
        // Count all geometries as a single id
        if (packet_data->world_mesh_data[i].unique_id > highest_instance_id)
            highest_instance_id = packet_data->world_mesh_data[i].unique_id;
    }

    // Iterate all terrains in world data
    u32 terrain_geometry_count = !packet_data->terrain_mesh_data ? 0 : darray_length(packet_data->terrain_mesh_data);

    // Iterate all geometries in terrain data
    for (u32 i = 0; i < terrain_geometry_count; ++i)
    {
        darray_push(out_packet->terrain_geometries, packet_data->terrain_mesh_data[i]);

        // Count all geometries as a single id
        if (packet_data->terrain_mesh_data[i].unique_id > highest_instance_id)
            highest_instance_id = packet_data->terrain_mesh_data[i].unique_id;
    }

    // Iterate all meshes in UI data
    for (u32 i = 0; i < packet_data->ui_mesh_data.mesh_count; ++i)
    {
        mesh* m = packet_data->ui_mesh_data.meshes[i];
        for (u32 j = 0; j < m->geometry_count; ++j)
        {
            geometry_render_data render_data;
            geometry* g = m->geometries[j];
            render_data.material = g->material;
            render_data.vertex_count = g->vertex_count;
            render_data.vertex_buffer_offset = g->vertex_buffer_offset;
            render_data.index_count = g->index_count;
            render_data.index_buffer_offset = g->index_buffer_offset;
            // TODO: Get from hierarchy
            // render_data.model = xform_world_get(m->xform);
            render_data.unique_id = m->id.uniqueid;
            darray_push(out_packet->geometries, render_data);
            out_packet->geometry_count++;
        }
        // Count all geometries as a single id
        if (m->id.uniqueid > highest_instance_id)
            highest_instance_id = m->id.uniqueid;
    }

    // Count texts as well
    // for (u32 i = 0; i < packet_data->text_count; ++i)
    // {
    //     if (packet_data->texts[i]->id.uniqueid > highest_instance_id)
    //         highest_instance_id = packet_data->texts[i]->id.uniqueid;
    // }

    i32 required_instance_count = highest_instance_id + 1;

    // Verify instance resources exist
    if (required_instance_count > internal_data->instance_count)
    {
        u32 diff = required_instance_count - internal_data->instance_count;
        for (u32 i = 0; i < diff; ++i)
            acquire_shader_instances(self);
    }

    // Copy over packet data
    bcopy_memory(out_packet->extended_data, packet_data, sizeof(pick_packet_data));

    return true;
}

void render_view_pick_on_packet_destroy(const struct render_view* self, struct render_view_packet* packet)
{
    darray_destroy(packet->geometries);
    darray_destroy(packet->terrain_geometries);
    bzero_memory(packet, sizeof(render_view_packet));
}

b8 render_view_pick_on_render(const struct render_view* self, const struct render_view_packet* packet, struct frame_data* p_frame_data)
{
    render_view_pick_internal_data* data = self->internal_data;

    // Bind viewport
    renderer_active_viewport_set(packet->vp);

    u32 p = 0;
    renderpass* pass = &self->passes[p]; // First pass

    if (p_frame_data->render_target_index == 0)
    {
        pick_packet_data* packet_data = (pick_packet_data*)packet->extended_data;
        if (!packet_data)
            return true;

        // Reset
        u64 count = darray_length(data->instance_updated);
        for (u64 i = 0; i < count; ++i)
            data->instance_updated[i] = false;

        if (!renderer_renderpass_begin(pass, &pass->targets[p_frame_data->render_target_index]))
        {
            BERROR("render_view_ui_on_render pass index %u failed to start", p);
            return false;
        }

        u64 current_instance_id = 0;

        // World
        if (!shader_system_use_by_id(data->world_shader_info.s->id))
        {
            BERROR("Failed to use world pick shader. Render frame failed");
            return false;
        }

        // Apply globals
        viewport* v = renderer_active_viewport_get();
        if (!shader_system_uniform_set_by_location(data->world_shader_info.projection_location, &v->projection))
            BERROR("Failed to apply projection matrix");
        if (!shader_system_uniform_set_by_location(data->world_shader_info.view_location, &data->world_shader_info.view))
            BERROR("Failed to apply view matrix");
        shader_system_apply_global(true, p_frame_data);

        // Draw geometries. Start from 0 since world geometries are added first, and stop at the world geometry count
        u32 world_geometry_count = !packet_data->world_mesh_data ? 0 : darray_length(packet_data->world_mesh_data);
        for (u32 i = 0; i < world_geometry_count; ++i)
        {
            geometry_render_data* geo = &packet->geometries[i];
            current_instance_id = geo->unique_id;

            shader_system_bind_instance(current_instance_id);

            // Get color based on id
            vec3 id_color;
            u32 r, g, b;
            u32_to_rgb(geo->unique_id, &r, &g, &b);
            rgb_u32_to_vec3(r, g, b, &id_color);
            if (!shader_system_uniform_set_by_location(data->world_shader_info.id_color_location, &id_color))
            {
                BERROR("Failed to apply id color uniform");
                return false;
            }

            b8 needs_update = !data->instance_updated[current_instance_id];
            shader_system_apply_instance(needs_update, p_frame_data);
            data->instance_updated[current_instance_id] = true;

            // Apply locals
            if (!shader_system_uniform_set_by_location(data->world_shader_info.model_location, &geo->model))
            {
                BERROR("Failed to apply model matrix for world geometry");
            }

            renderer_geometry_draw(&packet->geometries[i]);
        }
        // End world geometries

        // Terrain geometries
        if (!shader_system_use_by_id(data->terrain_shader_info.s->id))
        {
            BERROR("Failed to use terrain pick shader. Render frame failed");
            return false;
        }

        // Apply globals
        if (!shader_system_uniform_set_by_location(data->terrain_shader_info.projection_location, &v->projection))
            BERROR("Failed to apply projection matrix");
        if (!shader_system_uniform_set_by_location(data->terrain_shader_info.view_location, &data->terrain_shader_info.view))
            BERROR("Failed to apply view matrix");
        shader_system_apply_global(true, p_frame_data);

        // Draw geometries. Start from 0 since terrain geometries are added first, and stop at the terrain geometry count
        u32 terrain_geometry_count = !packet_data->terrain_mesh_data ? 0 : darray_length(packet_data->terrain_mesh_data);
        for (u32 i = 0; i < terrain_geometry_count; ++i)
        {
            geometry_render_data* geo = &packet->terrain_geometries[i];
            current_instance_id = geo->unique_id;

            shader_system_bind_instance(current_instance_id);

            // Get color based on id
            vec3 id_color;
            u32 r, g, b;
            u32_to_rgb(geo->unique_id, &r, &g, &b);
            rgb_u32_to_vec3(r, g, b, &id_color);
            if (!shader_system_uniform_set_by_location(data->terrain_shader_info.id_color_location, &id_color))
            {
                BERROR("Failed to apply id color uniform");
                return false;
            }

            b8 needs_update = !data->instance_updated[current_instance_id];
            shader_system_apply_instance(needs_update, p_frame_data);
            data->instance_updated[current_instance_id] = true;

            // Apply locals
            if (!shader_system_uniform_set_by_location(data->terrain_shader_info.model_location, &geo->model))
            {
                BERROR("Failed to apply model matrix for terrain geometry");
            }

            renderer_geometry_draw(&packet->terrain_geometries[i]);
        }

        if (!renderer_renderpass_end(pass))
        {
            BERROR("render_view_pick_on_render pass index %u failed to end", p);
            return false;
        }

        p++;
        pass = &self->passes[p]; // Second pass

        if (!renderer_renderpass_begin(pass, &pass->targets[p_frame_data->render_target_index]))
        {
            BERROR("render_view_pick_on_render pass index %u failed to start", p);
            return false;
        }

        // UI
        if (!shader_system_use_by_id(data->ui_shader_info.s->id))
        {
            BERROR("Failed to use material shader. Render frame failed");
            return false;
        }

        // Apply globals
        // TODO: This won't work as a single view
        BFATAL("Cannot use pick pass without it being split into UI/World first due to viewport changes");
        // if (!shader_system_uniform_set_by_location(data->ui_shader_info.projection_location, &data->ui_shader_info.projection))
        //     BERROR("Failed to apply projection matrix");
        if (!shader_system_uniform_set_by_location(data->ui_shader_info.view_location, &data->ui_shader_info.view))
            BERROR("Failed to apply view matrix");
        shader_system_apply_global(true, p_frame_data);

        // Draw geometries. Start off where world geometries left off
        for (u32 i = world_geometry_count; i < packet->geometry_count; ++i)
        {
            geometry_render_data* geo = &packet->geometries[i];
            current_instance_id = geo->unique_id;

            shader_system_bind_instance(current_instance_id);

            // Get color based on id
            vec3 id_color;
            u32 r, g, b;
            u32_to_rgb(geo->unique_id, &r, &g, &b);
            rgb_u32_to_vec3(r, g, b, &id_color);
            if (!shader_system_uniform_set_by_location(data->ui_shader_info.id_color_location, &id_color))
            {
                BERROR("Failed to apply id color uniform");
                return false;
            }

            b8 needs_update = !data->instance_updated[current_instance_id];
            shader_system_apply_instance(needs_update, p_frame_data);
            data->instance_updated[current_instance_id] = true;

            // Apply the locals
            if (!shader_system_uniform_set_by_location(data->ui_shader_info.model_location, &geo->model))
                BERROR("Failed to apply model matrix for text");

            renderer_geometry_draw(&packet->geometries[i]);
        }

        // Draw bitmap text
        // for (u32 i = 0; i < packet_data->text_count; ++i)
        // {
        //     ui_text* text = packet_data->texts[i];
        //     current_instance_id = text->id.uniqueid;
        //     shader_system_bind_instance(current_instance_id);

        //     // Get color based on id
        //     vec3 id_color;
        //     u32 r, g, b;
        //     u32_to_rgb(text->id.uniqueid, &r, &g, &b);
        //     rgb_u32_to_vec3(r, g, b, &id_color);
        //     if (!shader_system_uniform_set_by_location(data->ui_shader_info.id_color_location, &id_color))
        //     {
        //         BERROR("Failed to apply id color uniform");
        //         return false;
        //     }

        //     shader_system_apply_instance(true);

        //     // Apply locals
        //     mat4 model = transform_world_get(&text->transform);
        //     if (!shader_system_uniform_set_by_location(data->ui_shader_info.model_location, &model))
        //         BERROR("Failed to apply model matrix for text");

        //     ui_text_draw(text);
        // }

        if (!renderer_renderpass_end(pass))
        {
            BERROR("render_view_ui_on_render pass index %u failed to end", p);
            return false;
        }
    }

    // Read pixel data
    texture* t = &data->color_target_attachment_texture;

    // Read pixel at the mouse coordinate
    u8 pixel_rgba[4] = {0};
    u8* pixel = &pixel_rgba[0];

    // Clamp to image size
    u16 x_coord = BCLAMP(data->mouse_x, 0, self->width - 1);
    u16 y_coord = BCLAMP(data->mouse_y, 0, self->height - 1);
    renderer_texture_read_pixel(t, x_coord, y_coord, &pixel);

    // Extract id from sampled color
    u32 id = INVALID_ID;
    rgbu_to_u32(pixel[0], pixel[1], pixel[2], &id);
    if (id == 0x00FFFFFF)
    {
        // This is pure white
        id = INVALID_ID;
    }

    event_context context;
    context.data.u32[0] = id;
    event_fire(EVENT_CODE_OBJECT_HOVER_ID_CHANGED, 0, context);

    return true;
}

b8 render_view_pick_attachment_target_regenerate(struct render_view* self, u32 pass_index, struct render_target_attachment* attachment)
{
    render_view_pick_internal_data* data = self->internal_data;

    if (attachment->type == RENDER_TARGET_ATTACHMENT_TYPE_COLOR)
    {
        attachment->texture = &data->color_target_attachment_texture;
    }
    else if (attachment->type & RENDER_TARGET_ATTACHMENT_TYPE_DEPTH || attachment->type & RENDER_TARGET_ATTACHMENT_TYPE_STENCIL)
    {
        attachment->texture = &data->depth_target_attachment_texture;
    }
    else
    {
        BERROR("Unsupported attachment type 0x%x", attachment->type);
        return false;
    }

    if (pass_index == 1)
    {
        // Do not need to regenerate for both passes since they both use the same attachment
        // Just attach it and move on
        return true;
    }

    // Destroy current attachment if it exists
    if (attachment->texture->internal_data)
    {
        renderer_texture_destroy(attachment->texture);
        bzero_memory(attachment->texture, sizeof(texture));
    }

    // Setup new texture
    // Generate UUID to act as texture name
    uuid texture_name_uuid = uuid_generate();

    u32 width = self->width;
    u32 height = self->height;
    b8 has_transparency = false;  // TODO: make configurable

    attachment->texture->id = INVALID_ID;
    attachment->texture->type = TEXTURE_TYPE_2D;
    string_ncopy(attachment->texture->name, texture_name_uuid.value, TEXTURE_NAME_MAX_LENGTH);
    attachment->texture->width = width;
    attachment->texture->height = height;
    attachment->texture->channel_count = 4;  // TODO: make configurable
    attachment->texture->generation = INVALID_ID;
    attachment->texture->flags |= has_transparency ? TEXTURE_FLAG_HAS_TRANSPARENCY : 0;
    attachment->texture->flags |= TEXTURE_FLAG_IS_WRITEABLE;
    if (attachment->type == RENDER_TARGET_ATTACHMENT_TYPE_DEPTH)
        attachment->texture->flags |= TEXTURE_FLAG_DEPTH;
    attachment->texture->internal_data = 0;

    renderer_texture_create_writeable(attachment->texture);

    return true;
}
*/