#include "render_view_world.h"
#include "core/logger.h"
#include "core/bmemory.h"
#include "core/event.h"
#include "math/bmath.h"
#include "math/transform.h"
#include "containers/darray.h"
#include "systems/material_system.h"
#include "systems/shader_system.h"
#include "systems/camera_system.h"
#include "renderer/renderer_frontend.h"

typedef struct render_view_world_internal_data
{
    u32 shader_id;
    f32 fov;
    f32 near_clip;
    f32 far_clip;
    mat4 projection_matrix;
    camera* world_camera;
    vec4 ambient_color;
    u32 render_mode;
} render_view_world_internal_data;

typedef struct geometry_distance
{
    geometry_render_data g;
    f32 distance;
} geometry_distance;

static void quick_sort(geometry_distance arr[], i32 low_index, i32 high_index, b8 ascending);

static b8 render_view_on_event(u16 code, void* sender, void* listener_inst, event_context context)
{
    render_view* self = (render_view*)listener_inst;
    if (!self)
        return false;
    render_view_world_internal_data* data = (render_view_world_internal_data*)self->internal_data;
    if (!data)
        return false;

    switch (code)
    {
        case EVENT_CODE_SET_RENDER_MODE:
        {
            i32 mode = context.data.i32[0];
            switch (mode)
            {
                default:
                case RENDERER_VIEW_MODE_DEFAULT:
                    BDEBUG("Renderer mode set to default");
                    data->render_mode = RENDERER_VIEW_MODE_DEFAULT;
                    break;
                case RENDERER_VIEW_MODE_LIGHTING:
                    BDEBUG("Renderer mode set to lighting");
                    data->render_mode = RENDERER_VIEW_MODE_LIGHTING;
                    break;
                case RENDERER_VIEW_MODE_NORMALS:
                    BDEBUG("Renderer mode set to normals");
                    data->render_mode = RENDERER_VIEW_MODE_NORMALS;
                    break;
            }
            return true;
        }
    }

    return false;
}

b8 render_view_world_on_create(struct render_view* self)
{
    if (self)
    {
        self->internal_data = ballocate(sizeof(render_view_world_internal_data), MEMORY_TAG_RENDERER);
        render_view_world_internal_data* data = self->internal_data;
        // Get either custom shader override or defined default
        data->shader_id = shader_system_get_id(self->custom_shader_name ? self->custom_shader_name : "Shader.Builtin.Material");
        // TODO: Set from configuration
        data->near_clip = 0.1f;
        data->far_clip = 1000.0f;
        data->fov = deg_to_rad(45.0f);

        // Default
        data->projection_matrix = mat4_perspective(data->fov, 16 / 9.0f, data->near_clip, data->far_clip);

        data->world_camera = camera_system_get_default();

        // TODO: Obtain from scene
        data->ambient_color = (vec4){0.25f, 0.25f, 0.25f, 1.0f};

        // Listen for mode changes
        if (!event_register(EVENT_CODE_SET_RENDER_MODE, self, render_view_on_event))
        {
            BERROR("Unable to listen for render mode set event, creation failed");
            return false;
        }
        return true;
    }

    BERROR("render_view_world_on_create - Requires a valid pointer to a view");
    return false;
}

void render_view_world_on_destroy(struct render_view* self)
{
    if (self && self->internal_data)
    {
        event_unregister(EVENT_CODE_SET_RENDER_MODE, self, render_view_on_event);
        bfree(self->internal_data, sizeof(render_view_world_internal_data), MEMORY_TAG_RENDERER);
        self->internal_data = 0;
    }
}

void render_view_world_on_resize(struct render_view* self, u32 width, u32 height)
{
    // Check if different. If so, regenerate projection matrix
    if (width != self->width || height != self->height)
    {
        render_view_world_internal_data* data = self->internal_data;

        self->width = width;
        self->height = height;
        f32 aspect = (f32)self->width / self->height;
        data->projection_matrix = mat4_perspective(data->fov, aspect, data->near_clip, data->far_clip);

        for (u32 i = 0; i < self->renderpass_count; ++i)
        {
            self->passes[i]->render_area.x = 0;
            self->passes[i]->render_area.y = 0;
            self->passes[i]->render_area.z = width;
            self->passes[i]->render_area.w = height;
        }
    }
}

b8 render_view_world_on_build_packet(const struct render_view* self, void* data, struct render_view_packet* out_packet)
{
    if (!self || !data || !out_packet)
    {
        BWARN("render_view_world_on_build_packet requires valid pointer to view, packet, and data");
        return false;
    }

    mesh_packet_data* mesh_data = (mesh_packet_data*)data;
    render_view_world_internal_data* internal_data = (render_view_world_internal_data*)self->internal_data;

    out_packet->geometries = darray_create(geometry_render_data);
    out_packet->view = self;

    // Set matrices, etc
    out_packet->projection_matrix = internal_data->projection_matrix;
    out_packet->view_matrix = camera_view_get(internal_data->world_camera);
    out_packet->view_position = camera_position_get(internal_data->world_camera);
    out_packet->ambient_color = internal_data->ambient_color;

    // Obtain all geometries from the current scene
    geometry_distance* geometry_distances = darray_create(geometry_distance);

    for (u32 i = 0; i < mesh_data->mesh_count; ++i)
    {
        mesh* m = &mesh_data->meshes[i];
        mat4 model = transform_get_world(&m->transform);

        for (u32 j = 0; j < m->geometry_count; ++j)
        {
            geometry_render_data render_data;
            render_data.geometry = m->geometries[j];
            render_data.model = model;

            if ((m->geometries[j]->material->diffuse_map.texture->flags & TEXTURE_FLAG_HAS_TRANSPARENCY) == 0)
            {
                // Only add meshes with no transparency
                darray_push(out_packet->geometries, render_data);
                out_packet->geometry_count++;
            }
            else
            {
                // For meshes with transparency, add them to a separate list to be sorted by distance
                // NOTE: This isn't perfect for translucent meshes that intersect, but is enough for now
                vec3 center = vec3_transform(render_data.geometry->center, model);
                f32 distance = vec3_distance(center, internal_data->world_camera->position);

                geometry_distance gdist;
                gdist.distance = babs(distance);
                gdist.g = render_data;

                darray_push(geometry_distances, gdist);
            }
        }
    }

    // Sort distances
    u32 geometry_count = darray_length(geometry_distances);
    quick_sort(geometry_distances, 0, geometry_count - 1, false);

    // Add them to the packet geometry
    for (u32 i = 0; i < geometry_count; ++i)
    {
        darray_push(out_packet->geometries, geometry_distances[i].g);
        out_packet->geometry_count++;
    }

    return true;
}

b8 render_view_world_on_render(const struct render_view* self, const struct render_view_packet* packet, u64 frame_number, u64 render_target_index)
{
    render_view_world_internal_data* data = self->internal_data;
    u32 shader_id = data->shader_id;

    for (u32 p = 0; p < self->renderpass_count; ++p)
    {
        renderpass* pass = self->passes[p];
        if (!renderer_renderpass_begin(pass, &pass->targets[render_target_index]))
        {
            BERROR("render_view_world_on_render pass index %u failed to start", p);
            return false;
        }

        if (!shader_system_use_by_id(shader_id)) {
            BERROR("Failed to use material shader. Render frame failed");
            return false;
        }

        // Apply globals
        if (!material_system_apply_global(shader_id, frame_number, &packet->projection_matrix, &packet->view_matrix, &packet->ambient_color, &packet->view_position, data->render_mode))
        {
            BERROR("Failed to use apply globals for material shader. Render frame failed");
            return false;
        }

        // Draw geometries
        u32 count = packet->geometry_count;
        for (u32 i = 0; i < count; ++i)
        {
            material* m = 0;
            if (packet->geometries[i].geometry->material)
                m = packet->geometries[i].geometry->material;
            else
                m = material_system_get_default();

            // Update material if it hasn't already been this frame
            b8 needs_update = m->render_frame_number != frame_number;
            if (!material_system_apply_instance(m, needs_update))
            {
                BWARN("Failed to apply material '%s'. Skipping draw", m->name);
                continue;
            }
            else
            {
                // Sync frame number
                m->render_frame_number = frame_number;
            }

            // Apply locals
            material_system_apply_local(m, &packet->geometries[i].model);

            // Draw it
            renderer_draw_geometry(&packet->geometries[i]);
        }

        if (!renderer_renderpass_end(pass))
        {
            BERROR("render_view_world_on_render pass index %u failed to end", p);
            return false;
        }
    }

    return true;
}

// Quicksort for geometry_distance

static void swap(geometry_distance* a, geometry_distance* b)
{
    geometry_distance temp = *a;
    *a = *b;
    *b = temp;
}

static i32 partition(geometry_distance arr[], i32 low_index, i32 high_index, b8 ascending)
{
    geometry_distance pivot = arr[high_index];
    i32 i = (low_index - 1);

    for (i32 j = low_index; j <= high_index - 1; ++j)
    {
        if (ascending)
        {
            if (arr[j].distance < pivot.distance)
            {
                ++i;
                swap(&arr[i], &arr[j]);
            }
        }
        else
        {
            if (arr[j].distance > pivot.distance)
            {
                ++i;
                swap(&arr[i], &arr[j]);
            }
        }
    }
    swap(&arr[i + 1], &arr[high_index]);
    return i + 1;
}

static void quick_sort(geometry_distance arr[], i32 low_index, i32 high_index, b8 ascending)
{
    if (low_index < high_index)
    {
        i32 partition_index = partition(arr, low_index, high_index, ascending);

        // Independently sort elements before and after the partition index
        quick_sort(arr, low_index, partition_index - 1, ascending);
        quick_sort(arr, partition_index + 1, high_index, ascending);
    }
}
