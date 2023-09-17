#include "renderer_frontend.h"
#include "renderer_backend.h"
#include "core/logger.h"
#include "core/bmemory.h"
#include "math/bmath.h"
#include "resources/resource_types.h"

typedef struct renderer_system_state
{
    renderer_backend backend;
    mat4 projection;
    mat4 view;
    f32 near_clip;
    f32 far_clip;

    texture default_texture;
} renderer_system_state;

static renderer_system_state* state_ptr;

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name)
{
    *memory_requirement = sizeof(renderer_system_state);
    if (state == 0)
        return true;
    state_ptr = state;

    // TODO: make this configurable     V
    renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, &state_ptr->backend);
    state_ptr->backend.frame_number = 0;

    if (!state_ptr->backend.initialize(&state_ptr->backend, application_name))
    {
        BFATAL("Renderer backend failed to initialize. Shutting down");
        return false;
    }

    state_ptr->near_clip = 0.1f;
    state_ptr->far_clip = 1000.0f;
    state_ptr->projection = mat4_perspective(deg_to_rad(45.0f), 16/9.0f, state_ptr->near_clip, state_ptr->far_clip);

    state_ptr->view = mat4_translation((vec3){0, 0, 30.0f});
    state_ptr->view = mat4_inverse(state_ptr->view);

    // NOTE: Create default texture, a 256x256 blue/white checkerboard pattern
    BTRACE("Creating default texture...");
    const u32 tex_dimension = 256;
    const u32 channels = 4;
    const u32 pixel_count = tex_dimension * tex_dimension;
    u8 pixels[pixel_count * channels];
    //u8* pixels = ballocate(sizeof(u8) * pixel_count * bpp, MEMORY_TAG_TEXTURE);
    bset_memory(pixels, 255, sizeof(u8) * pixel_count * channels);

    // Each pixel
    for (u64 row = 0; row < tex_dimension; ++row)
    {
        for (u64 col = 0; col < tex_dimension; ++col)
        {
            u64 index = (row * tex_dimension) + col;
            u64 index_bpp = index * channels;
            if (row % 2)
            {
                if (col % 2)
                {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
            else
            {
                if (!(col % 2))
                {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
        }
    }
    renderer_create_texture(
        "default",
        false,
        tex_dimension,
        tex_dimension,
        4,
        pixels,
        false,
        &state_ptr->default_texture);

    return true;
}

void renderer_system_shutdown(void* state)
{
    if (state_ptr)
    {
        renderer_destroy_texture(&state_ptr->default_texture);
        state_ptr->backend.shutdown(&state_ptr->backend);
    }
    state_ptr = 0;
}

b8 renderer_begin_frame(f32 delta_time)
{
    if (!state_ptr)
        return false;
    return state_ptr->backend.begin_frame(&state_ptr->backend, delta_time);
}

b8 renderer_end_frame(f32 delta_time)
{
    if (!state_ptr)
        return false;
    b8 result = state_ptr->backend.end_frame(&state_ptr->backend, delta_time);
    state_ptr->backend.frame_number++;
    return result;
}

void renderer_on_resized(u16 width, u16 height)
{
    if (state_ptr)
    {
        state_ptr->projection = mat4_perspective(deg_to_rad(45.0f), width / (f32)height, state_ptr->near_clip, state_ptr->far_clip);
        state_ptr->backend.resized(&state_ptr->backend, width, height);
    }
    else
    {
        BWARN("Renderer backend does not exist to accept resize: %i %i", width, height);
    }
}

b8 renderer_draw_frame(render_packet* packet)
{
    // If the begin frame returned successfully, mid-frame operations may continue
    if (renderer_begin_frame(packet->delta_time))
    {
        state_ptr->backend.update_global_state(state_ptr->projection, state_ptr->view, vec3_zero(), vec4_one(), 0);

        mat4 model = mat4_translation((vec3){0, 0, 0});
        // static f32 angle = 0.01f;
        // angle += 0.001f;
        // quat rotation = quat_from_axis_angle(vec3_forward(), angle, false);
        // mat4 model = quat_to_rotation_matrix(rotation, vec3_zero());
        geometry_render_data data = {};
        data.object_id = 0; // TODO: actual object id
        data.model = model;
        data.textures[0] = &state_ptr->default_texture;
        state_ptr->backend.update_object(data);

        // End frame. If this fails, it is likely unrecoverable
        b8 result = renderer_end_frame(packet->delta_time);

        if (!result)
        {
            BERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }
    
    return true;
}

void renderer_set_view(mat4 view)
{
    state_ptr->view = view;
}

void renderer_create_texture(const char* name, b8 auto_release, i32 width, i32 height, i32 channel_count, const u8* pixels, b8 has_transparency, struct texture* out_texture)
{
    state_ptr->backend.create_texture(name, auto_release, width, height, channel_count, pixels, has_transparency, out_texture);
}

void renderer_destroy_texture(struct texture* texture)
{
    state_ptr->backend.destroy_texture(texture);
}
