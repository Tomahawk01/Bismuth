#include "texture_system.h"

#include "assets/basset_types.h"
#include "core/engine.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "resources/resource_types.h"
#include "systems/bresource_system.h"
#include "strings/bname.h"
#include "strings/bstring.h"

typedef struct texture_system_state
{
    texture_system_config config;

    bresource_texture* default_bresource_texture;
    bresource_texture* default_bresource_diffuse_texture;
    bresource_texture* default_bresource_specular_texture;
    bresource_texture* default_bresource_normal_texture;
    bresource_texture* default_bresource_combined_texture;
    bresource_texture* default_bresource_cube_texture;
    bresource_texture* default_bresource_terrain_texture;

    // A convenience pointer to the renderer system state
    struct renderer_system_state* renderer;

    struct bresource_system_state* bresource_system;
} texture_system_state;

typedef struct texture_load_params
{
    char* resource_name;
    bresource_texture* out_texture;
    bresource_texture temp_texture;
    resource image_resource;
} texture_load_params;

typedef struct texture_load_layered_params
{
    char* name;
    u32 layer_count;
    char** layer_names;
    bresource_texture* out_texture;
} texture_load_layered_params;

typedef enum texture_load_job_code
{
    TEXTURE_LOAD_JOB_CODE_FIRST_QUERY_FAILED,
    TEXTURE_LOAD_JOB_CODE_RESOURCE_LOAD_FAILED,
    TEXTURE_LOAD_JOB_CODE_RESOURCE_DIMENSION_MISMATCH,
} texture_load_job_code;

typedef struct texture_load_layered_result
{
    char* name;
    u32 layer_count;
    bresource_texture* out_texture;
    u64 data_block_size;
    u8* data_block;
    bresource_texture temp_texture;
    texture_load_job_code result_code;
} texture_load_layered_result;

static texture_system_state* state_ptr = 0;

static b8 create_default_textures(texture_system_state* state);
static void release_default_textures(texture_system_state* state);
static void increment_generation(bresource_texture* t);
static void invalidate_texture(bresource_texture* t);
static b8 texture_system_is_default_texture(bresource_texture* t);

static bresource_texture* default_texture_by_name(texture_system_state* state, bname name);
static bresource_texture* request_writeable_arrayed(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency, bresource_texture_type type, u16 array_size, b8 is_depth, b8 multiframe_buffering);

b8 texture_system_initialize(u64* memory_requirement, void* state, void* config)
{
    texture_system_config* typed_config = (texture_system_config*)config;
    if (typed_config->max_texture_count == 0)
    {
        BFATAL("texture_system_initialize - typed_config->max_texture_count must be > 0");
        return false;
    }

    *memory_requirement = sizeof(texture_system_state);

    if (!state)
        return true;
    
    BDEBUG("Initializing texture system...");

    state_ptr = state;
    state_ptr->config = *typed_config;

    // Keep a pointer to the renderer system state
    state_ptr->renderer = engine_systems_get()->renderer_system;
    state_ptr->bresource_system = engine_systems_get()->bresource_state;

    // Create default textures for use in system
    create_default_textures(state_ptr);

    BDEBUG("Texture system initialization complete");

    return true;
}

void texture_system_shutdown(void* state)
{
    if (state_ptr)
    {
        release_default_textures(state_ptr);

        state_ptr->renderer = 0;
        state_ptr = 0;
    }
}

bresource_texture* texture_system_request(bname name, bname package_name, void* listener, PFN_resource_loaded_user_callback callback)
{
    texture_system_state* state = engine_systems_get()->texture_system;

    // Check that name is not the name of a default texture. If it is, immediately make the callback with the appropriate default texture and return it
    bresource_texture* t = default_texture_by_name(state, name);
    if (t)
    {
        if (callback)
            callback((bresource*)t, listener);
        return t;
    }
    // If not default, request the resource from the resource system
    bresource_texture_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_TEXTURE;
    request.base.listener_inst = listener;
    request.base.user_callback = callback;

    request.base.assets = array_bresource_asset_info_create(1);
    request.base.assets.data[0].type = BASSET_TYPE_IMAGE;
    request.base.assets.data[0].package_name = package_name;
    request.base.assets.data[0].asset_name = name;

    request.array_size = 1;
    request.texture_type = BRESOURCE_TEXTURE_TYPE_2D;
    request.flags = 0;
    request.flip_y = true;

    t = (bresource_texture*)bresource_system_request(state->bresource_system, name, (bresource_request_info*)&request);
    if (!t)
        BERROR("Failed to properly request resource for texture '%s'", bname_string_get(name));

    return t;
}

bresource_texture* texture_system_request_cube(bname name, b8 auto_release, b8 multiframe_buffering, void* listener, PFN_resource_loaded_user_callback callback)
{
    texture_system_state* state = engine_systems_get()->texture_system;

    // If requesting the default cube texture name, just return it
    if (name == state->default_bresource_cube_texture->base.name)
        return state->default_bresource_cube_texture;

    if (name == INVALID_BNAME)
    {
        BWARN("texture_system_request_cube - name supplied is invalid. Returning default cubemap instead");
        return state->default_bresource_cube_texture;
    }

    // If not default, request the resource from the resource system
    bresource_texture_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_TEXTURE;
    request.base.listener_inst = listener;
    request.base.user_callback = callback;

    request.base.assets = array_bresource_asset_info_create(6);

    // +X,-X,+Y,-Y,+Z,-Z in cubemap space, which is LH y-down
    // Build out image side asset names. Order is important here
    // name_r Right
    // name_l Left
    // name_u Up
    // name_d Down
    // name_f Front
    // name_b Back
    const char* sides = "rludfb";
    const char* base_name = bname_string_get(name);
    for (u8 i = 0; i < 6; ++i)
    {
        const char* buf = string_format("%s_%c", base_name, sides[i]);
        bname side_name = bname_create(buf);
        string_free(buf);

        request.base.assets.data[i].type = BASSET_TYPE_IMAGE;
        request.base.assets.data[i].package_name = INVALID_BNAME; // TODO: automatic package name
        request.base.assets.data[i].asset_name = side_name;
    }

    request.array_size = 6;
    request.texture_type = BRESOURCE_TEXTURE_TYPE_CUBE;
    request.flags = multiframe_buffering ? BRESOURCE_TEXTURE_FLAG_RENDERER_BUFFERING : 0;
    request.flip_y = false;

    bresource_texture* t = (bresource_texture*)bresource_system_request(state->bresource_system, name, (bresource_request_info*)&request);
    if (!t)
        BERROR("Failed to properly request resource for cube texture '%s'", bname_string_get(name));

    return t;
}

bresource_texture* texture_system_request_cube_writeable(bname name, u32 dimension, b8 auto_release, b8 multiframe_buffering)
{
    texture_system_state* state = engine_systems_get()->texture_system;
    // If requesting the default cube texture name, just return it
    if (name == state->default_bresource_cube_texture->base.name)
        return state->default_bresource_cube_texture;

    if (name == INVALID_BNAME)
    {
        BWARN("texture_system_request_cube - name supplied is invalid. Returning default cubemap instead");
        return state->default_bresource_cube_texture;
    }
    return request_writeable_arrayed(name, dimension, dimension, BRESOURCE_TEXTURE_FORMAT_RGBA8, false, BRESOURCE_TEXTURE_TYPE_CUBE, 6, false, multiframe_buffering);
}

bresource_texture* texture_system_request_cube_depth(bname name, u32 dimension, b8 auto_release, b8 multiframe_buffering)
{
    texture_system_state* state = engine_systems_get()->texture_system;
    // If requesting the default cube texture name, just return it
    if (name == state->default_bresource_cube_texture->base.name)
        return state->default_bresource_cube_texture;

    if (name == INVALID_BNAME)
    {
        BWARN("texture_system_request_cube - name supplied is invalid. Returning default cubemap instead");
        return state->default_bresource_cube_texture;
    }
    return request_writeable_arrayed(name, dimension, dimension, BRESOURCE_TEXTURE_FORMAT_RGBA8, false, BRESOURCE_TEXTURE_TYPE_CUBE, 6, true, multiframe_buffering);
}

bresource_texture* texture_system_request_writeable(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency, b8 multiframe_buffering)
{
    return request_writeable_arrayed(name, width, height, format, has_transparency, BRESOURCE_TEXTURE_TYPE_2D, 1, false, multiframe_buffering);
}

bresource_texture* texture_system_request_writeable_arrayed(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency, b8 multiframe_buffering, bresource_texture_type type, u16 array_size)
{
    return request_writeable_arrayed(name, width, height, format, has_transparency, type, array_size, false, multiframe_buffering);
}

bresource_texture* texture_system_request_depth(bname name, u32 width, u32 height, b8 multiframe_buffering)
{
    return request_writeable_arrayed(name, width, height, BRESOURCE_TEXTURE_FORMAT_RGBA8, false, BRESOURCE_TEXTURE_TYPE_2D, 1, true, multiframe_buffering);
}

bresource_texture* texture_system_request_depth_arrayed(bname name, u32 width, u32 height, u16 array_size, b8 multiframe_buffering)
{
    return request_writeable_arrayed(name, width, height, BRESOURCE_TEXTURE_FORMAT_RGBA8, false, BRESOURCE_TEXTURE_TYPE_2D_ARRAY, array_size, true, multiframe_buffering);
}

bresource_texture* texture_system_acquire_textures_as_arrayed(bname name, bname package_name, u32 layer_count, bname* layer_asset_names, b8 auto_release, b8 multiframe_buffering, void* listener, PFN_resource_loaded_user_callback callback)
{
    if (layer_count < 1)
    {
        BERROR("Must contain at least one layer");
        return 0;
    }

    texture_system_state* state = engine_systems_get()->texture_system;

    // Check that name is not the name of a default texture. If it is, immediately make the callback with the appropriate default texture and return it
    bresource_texture* t = default_texture_by_name(state, name);
    if (t)
    {
        if (callback)
            callback((bresource*)t, listener);
        return t;
    }
    // If not default, request the resource from the resource system
    bresource_texture_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_TEXTURE;
    request.base.listener_inst = listener;
    request.base.user_callback = callback;

    request.base.assets = array_bresource_asset_info_create(layer_count);
    for (u32 i = 0; i < layer_count; ++i)
    {
        bresource_asset_info* asset = &request.base.assets.data[i];
        asset->type = BASSET_TYPE_IMAGE;
        asset->package_name = package_name;
        asset->asset_name = layer_asset_names[i];
    }

    request.array_size = layer_count;
    request.texture_type = BRESOURCE_TEXTURE_TYPE_2D_ARRAY;
    request.flags = 0;
    request.flip_y = true;

    t = (bresource_texture*)bresource_system_request(state->bresource_system, name, (bresource_request_info*)&request);
    if (!t)
        BERROR("Failed to properly request resource for arrayed texture '%s'", bname_string_get(name));

    return t;
}

void texture_system_release_resource(bresource_texture* t)
{
    struct bresource_system_state* resource_system = engine_systems_get()->bresource_state;
    texture_system_state* state = engine_systems_get()->texture_system;

    // Do nothing if this is a default texture
    if (t->base.name == state->default_bresource_texture->base.name ||
        t->base.name == state->default_bresource_diffuse_texture->base.name ||
        t->base.name == state->default_bresource_specular_texture->base.name ||
        t->base.name == state->default_bresource_normal_texture->base.name ||
        t->base.name == state->default_bresource_combined_texture->base.name ||
        t->base.name == state->default_bresource_cube_texture->base.name ||
        t->base.name == state->default_bresource_terrain_texture->base.name)
    {
        return;
    }

    bresource_system_release(resource_system, t->base.name);
}

b8 texture_system_resize(bresource_texture* t, u32 width, u32 height, b8 regenerate_internal_data)
{
    if (t)
    {
        if (!(t->flags & TEXTURE_FLAG_IS_WRITEABLE))
        {
            BWARN("texture_system_resize should not be called on textures that are not writeable");
            return false;
        }
        t->width = width;
        t->height = height;
        // FIXME: remove this requirement, and potentially the regenerate_internal_data flag as well
        // Only allow this for writeable textures that are not wrapped.
        // Wrapped textures can call texture_system_set_internal then call
        // this function to get the above parameter updates and generation update
        if (!(t->flags & TEXTURE_FLAG_IS_WRAPPED) && regenerate_internal_data)
        {
            // Regenerate internals for new size
            b8 result = renderer_texture_resize(state_ptr->renderer, t->renderer_texture_handle, width, height);
            increment_generation(t);
            return result;
        }
        return true;
    }
    return false;
}

b8 texture_system_write_data(bresource_texture* t, u32 offset, u32 size, void* data)
{
    if (t)
    {
        return renderer_texture_write_data(state_ptr->renderer, t->renderer_texture_handle, offset, size, data);
    }
    return false;
}

#define RETURN_TEXT_PTR_OR_NULL(texture, func_name)                                             \
    if (state_ptr)                                                                              \
        return &texture;                                                                        \
    BERROR("%s called before texture system initialization! Null pointer returned", func_name); \
    return 0;

static b8 texture_system_is_default_texture(bresource_texture* t)
{
    if (!state_ptr)
        return false;

    return (t == state_ptr->default_bresource_texture) ||
           (t == state_ptr->default_bresource_diffuse_texture) ||
           (t == state_ptr->default_bresource_normal_texture) ||
           (t == state_ptr->default_bresource_specular_texture) ||
           (t == state_ptr->default_bresource_combined_texture) ||
           (t == state_ptr->default_bresource_terrain_texture) ||
           (t == state_ptr->default_bresource_cube_texture);
}

const bresource_texture* texture_system_get_default_bresource_texture(struct texture_system_state* state)
{
    return state->default_bresource_texture;
}

const bresource_texture* texture_system_get_default_bresource_diffuse_texture(struct texture_system_state* state)
{
    return state->default_bresource_diffuse_texture;
}

const bresource_texture* texture_system_get_default_bresource_specular_texture(struct texture_system_state* state)
{
    return state->default_bresource_specular_texture;
}

const bresource_texture* texture_system_get_default_bresource_normal_texture(struct texture_system_state* state)
{
    return state->default_bresource_normal_texture;
}

const bresource_texture* texture_system_get_default_bresource_combined_texture(struct texture_system_state* state)
{
    return state->default_bresource_combined_texture;
}

const bresource_texture* texture_system_get_default_bresource_cube_texture(struct texture_system_state* state)
{
    return state->default_bresource_cube_texture;
}

const bresource_texture* texture_system_get_default_bresource_terrain_texture(struct texture_system_state* state)
{
    return state->default_bresource_terrain_texture;
}

bhandle texture_system_resource_get_internal_or_default(const bresource_texture* t, u32* out_generation)
{
    if (!t || !out_generation)
        return bhandle_invalid();

    texture_system_state* state = engine_systems_get()->texture_system;

    bhandle tex_handle = t->renderer_texture_handle;

    // Texture isn't loaded yet, use a default
    if (t->base.generation == INVALID_ID)
    {
        // Texture generations are always invalid for default textures, so check first if already using one
        // TODO: Default texture for bresource_texture
        // if (!texture_system_is_default_texture(t)) {

        // If not using one, grab the default by type. This is only here as a failsafe and to be used while assets are loading
        switch (t->type)
        {
        case TEXTURE_TYPE_2D:
            tex_handle = texture_system_get_default_bresource_texture(state)->renderer_texture_handle;
            break;
        case TEXTURE_TYPE_2D_ARRAY:
            // TODO: Making the assumption this is a terrain
            // Should acquire a new default texture with the corresponding number of layers instead
            tex_handle = texture_system_get_default_bresource_terrain_texture(state)->renderer_texture_handle;
            break;
        case TEXTURE_TYPE_CUBE:
            tex_handle = texture_system_get_default_bresource_cube_texture(state)->renderer_texture_handle;
            break;
        default:
            BWARN("Texture system failed to determine texture type while getting internal data. Falling back to 2D");
            tex_handle = texture_system_get_default_bresource_texture(state)->renderer_texture_handle;
            break;
        }
        //}

        // Since using a default texture, set the outward generation to invalid id
        *out_generation = INVALID_ID;
    }
    else
    {
        // Set the actual texture generation
        *out_generation = t->base.generation;
    }

    return tex_handle;
}

/* static b8 create_and_upload_texture(texture* t, const char* name, texture_type type, u32 width, u32 height, u8 channel_count, u8 mip_levels, u16 array_size, texture_flag_bits flags, u32 offset, u8* pixels)
{
    // Setup basic texture properties
    t->width = width;
    t->height = height;
    t->channel_count = channel_count;
    t->flags = flags;
    t->type = type;
    t->mip_levels = mip_levels;
    t->array_size = array_size;

    // Take a copy of the name
    t->name = string_duplicate(name);

    // Acquire backing renderer resources
    if (!renderer_texture_resources_acquire(state_ptr->renderer, t->name, t->type, t->width, t->height, t->channel_count, t->mip_levels, t->array_size, t->flags, &t->renderer_texture_handle))
    {
        string_free(t->name);
        BERROR("Failed to acquire renderer resources for default texture '%s'. See logs for details", name);

        // invalidate_texture
        bzero_memory(t, sizeof(texture));
        t->generation = INVALID_ID_U8;
        t->renderer_texture_handle = bhandle_invalid();
        // invalidate_texture(t);
        return false;
    }

    // Upload the texture data
    u32 size = t->width * t->height * t->channel_count * t->array_size;
    if (!renderer_texture_write_data(state_ptr->renderer, t->renderer_texture_handle, 0, size, pixels))
    {
        BERROR("Failed to write texture data for default texture '%s'", name);
        string_free(t->name);
        t->name = 0;
        // Since this failed, make sure to release the texture's backing renderer resources
        renderer_texture_resources_release(state_ptr->renderer, &t->renderer_texture_handle);
        // invalidate_texture
        bzero_memory(t, sizeof(texture));
        t->generation = INVALID_ID_U8;
        t->renderer_texture_handle = bhandle_invalid();
        // invalidate_texture(t);

        return false;
    }

    if (texture_system_is_default_texture(t))
    {
        // Default textures always have an invalid generation
        t->generation = INVALID_ID_U8;
    }
    else
    {
        // TODO: Check for upload success before incrementing. If successful (or pending success on frame workload), update texture generation
        // increment_generation(t);
        t->generation++;
    }

    return true;
} */

/* static b8 create_default_texture(texture* t, u8* pixels, u32 tex_dimension, const char* name)
{
    // Acquire internal texture resources and upload to GPU
    return create_and_upload_texture(t, name, TEXTURE_TYPE_2D, tex_dimension, tex_dimension, 4, 1, 1, 0, 0, pixels);
} */

/* static b8 create_default_cube_texture(texture* t, const char* name)
{
    const u32 tex_dimension = 16;
    const u32 channels = 4;
    const u32 layers = 6; // one per side
    const u32 pixel_count = tex_dimension * tex_dimension;
    u8 cube_side_pixels[16 * 16 * 4];
    bset_memory(cube_side_pixels, 255, sizeof(u8) * pixel_count * channels);

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
                    cube_side_pixels[index_bpp + 1] = 0;
                    cube_side_pixels[index_bpp + 2] = 0;
                }
            }
            else
            {
                if (!(col % 2))
                {
                    cube_side_pixels[index_bpp + 1] = 0;
                    cube_side_pixels[index_bpp + 2] = 0;
                }
            }
        }
    }

    // Copy the image side data (same on all sides) to the relevant portion of the pixel array
    u64 layer_size = sizeof(u8) * tex_dimension * tex_dimension * channels;
    u64 image_size = layer_size * layers;
    u8* pixels = ballocate(image_size, MEMORY_TAG_ARRAY);
    for (u8 i = 0; i < layers; ++i)
        bcopy_memory(pixels + layer_size * i, cube_side_pixels, layer_size);

    // Acquire internal texture resources and upload to GPU
    b8 result = create_and_upload_texture(t, name, TEXTURE_TYPE_CUBE, tex_dimension, tex_dimension, channels, 1, layers, 0, 0, pixels);

    // Cleanup pixels array
    bfree(pixels, image_size, MEMORY_TAG_ARRAY);
    pixels = 0;

    return result;
} */

/* static b8 create_default_layered_texture(texture* t, u32 layer_count, u8* all_layer_pixels, u32 tex_dimension, const char* name)
{
    // Acquire internal texture resources and upload to GPU
    return create_and_upload_texture(t, name, TEXTURE_TYPE_2D_ARRAY, tex_dimension, tex_dimension, 4, 1, layer_count, 0, 0, all_layer_pixels);
} */

bresource_texture* create_default_bresource_texture(texture_system_state* state, bname name, bresource_texture_type type, u32 tex_dimension, u8 layer_count, u8 channel_count, u32 pixel_array_size, u8* pixels)
{
    bresource_texture_request_info request = {0};
    bzero_memory(&request, sizeof(bresource_texture_request_info));
    request.texture_type = type;
    request.array_size = layer_count;
    request.flags = BRESOURCE_TEXTURE_FLAG_IS_WRITEABLE;
    request.pixel_data = array_bresource_texture_pixel_data_create(1);
    bresource_texture_pixel_data* px = &request.pixel_data.data[0];
    px->pixel_array_size = pixel_array_size;
    px->pixels = pixels;
    px->width = tex_dimension;
    px->height = tex_dimension;
    px->channel_count = channel_count;
    px->format = BRESOURCE_TEXTURE_FORMAT_RGBA8;
    px->mip_levels = 1;
    request.base.type = BRESOURCE_TYPE_TEXTURE;
    request.flip_y = false;
    bresource_texture* t = (bresource_texture*)bresource_system_request(state->bresource_system, name, (bresource_request_info*)&request);
    if (!t)
        BERROR("Failed to request resources for default texture");

    return t;
}

static b8 create_default_textures(texture_system_state* state)
{
    // NOTE: Create default texture. 256x256 blue/white checkerboard pattern
    // BTRACE("Creating default texture...");
    const u32 tex_dimension = 16;
    const u32 channels = 4;
    const u32 pixel_count = tex_dimension * tex_dimension;
    u8 pixels[16 * 16 * 4];
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

    // Default texture
    {
        BTRACE("Creating default resource texture...");

        // Request new resource texture
        u32 pixel_array_size = sizeof(u8) * pixel_count * channels;
        state->default_bresource_texture = create_default_bresource_texture(state, bname_create(DEFAULT_TEXTURE_NAME), BRESOURCE_TEXTURE_TYPE_2D, tex_dimension, 1, channels, pixel_array_size, pixels);
        if (!state->default_bresource_texture)
        {
            BERROR("Failed to request resources for default texture");
            return false;
        }
    }

    // Diffuse texture
    {
        BTRACE("Creating default diffuse texture...");

        u8 diff_pixels[16 * 16 * 4];
        // Default diffuse map is all white
        bset_memory(diff_pixels, 255, sizeof(u8) * 16 * 16 * 4);

        // Request new resource texture
        u32 pixel_array_size = sizeof(u8) * pixel_count * channels;
        state->default_bresource_diffuse_texture = create_default_bresource_texture(state, bname_create(DEFAULT_DIFFUSE_TEXTURE_NAME), BRESOURCE_TEXTURE_TYPE_2D, tex_dimension, 1, channels, pixel_array_size, diff_pixels);
        if (!state->default_bresource_diffuse_texture)
        {
            BERROR("Failed to request resources for default diffuse texture");
            return false;
        }
    }

    // Specular texture
    {
        BTRACE("Creating default specular texture...");
        u8 spec_pixels[16 * 16 * 4];
        // Default spec map is black (no specular)
        bset_memory(spec_pixels, 0, sizeof(u8) * 16 * 16 * 4);

        // Request new resource texture
        u32 pixel_array_size = sizeof(u8) * pixel_count * channels;
        state->default_bresource_specular_texture = create_default_bresource_texture(state, bname_create(DEFAULT_SPECULAR_TEXTURE_NAME), BRESOURCE_TEXTURE_TYPE_2D, tex_dimension, 1, channels, pixel_array_size, spec_pixels);
        if (!state->default_bresource_specular_texture)
        {
            BERROR("Failed to request resources for default specular texture");
            return false;
        }
    }

    // Normal texture
    u8 normal_pixels[16 * 16 * 4];  // w * h * channels
    {
        BTRACE("Creating default normal texture...");
        bset_memory(normal_pixels, 255, sizeof(u8) * 16 * 16 * 4);

        // Each pixel
        for (u64 row = 0; row < 16; ++row)
        {
            for (u64 col = 0; col < 16; ++col)
            {
                u64 index = (row * 16) + col;
                u64 index_bpp = index * channels;
                // Set blue, z-axis by default and alpha
                normal_pixels[index_bpp + 0] = 128;
                normal_pixels[index_bpp + 1] = 128;
            }
        }

        // Request new resource texture
        u32 pixel_array_size = sizeof(u8) * pixel_count * channels;
        state->default_bresource_normal_texture = create_default_bresource_texture(state, bname_create(DEFAULT_NORMAL_TEXTURE_NAME), BRESOURCE_TEXTURE_TYPE_2D, tex_dimension, 1, channels, pixel_array_size, normal_pixels);
        if (!state->default_bresource_normal_texture)
        {
            BERROR("Failed to request resources for default normal texture");
            return false;
        }
    }

    // Combined texture
    u8 combined_pixels[16 * 16 * 4];  // w * h * channels
    {
        BTRACE("Creating default combined (metallic, roughness, AO) texture...");
        bset_memory(combined_pixels, 255, sizeof(u8) * 16 * 16 * 4);

        // Each pixel
        for (u64 row = 0; row < 16; ++row)
        {
            for (u64 col = 0; col < 16; ++col)
            {
                u64 index = (row * 16) + col;
                u64 index_bpp = index * channels;
                combined_pixels[index_bpp + 0] = 0;   // Default for metallic is black
                combined_pixels[index_bpp + 1] = 128; // Default for roughness is medium grey
                combined_pixels[index_bpp + 2] = 255; // Default for AO is white
            }
        }

        // Request new resource texture
        u32 pixel_array_size = sizeof(u8) * pixel_count * channels;
        state->default_bresource_combined_texture = create_default_bresource_texture(state, bname_create(DEFAULT_COMBINED_TEXTURE_NAME), BRESOURCE_TEXTURE_TYPE_2D, tex_dimension, 1, channels, pixel_array_size, combined_pixels);
        if (!state->default_bresource_combined_texture)
        {
            BERROR("Failed to request resources for default combined texture");
            return false;
        }
    }

    // Cube texture
    {
        BTRACE("Creating default cube texture...");

        // New resource type
        const u32 tex_dimension = 16;
        const u32 channels = 4;
        const u32 layers = 6; // one per side
        const u32 pixel_count = tex_dimension * tex_dimension;
        u8 cube_side_pixels[16 * 16 * 4];
        bset_memory(cube_side_pixels, 255, sizeof(u8) * pixel_count * channels);

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
                        cube_side_pixels[index_bpp + 1] = 0;
                        cube_side_pixels[index_bpp + 2] = 0;
                    }
                }
                else
                {
                    if (!(col % 2))
                    {
                        cube_side_pixels[index_bpp + 1] = 0;
                        cube_side_pixels[index_bpp + 2] = 0;
                    }
                }
            }
        }

        // Copy the image side data (same on all sides) to the relevant portion of the pixel array
        u64 layer_size = sizeof(u8) * tex_dimension * tex_dimension * channels;
        u64 image_size = layer_size * layers;
        u8* pixels = ballocate(image_size, MEMORY_TAG_ARRAY);
        for (u8 i = 0; i < layers; ++i)
            bcopy_memory(pixels + layer_size * i, cube_side_pixels, layer_size);

        // Request new resource texture
        u32 pixel_array_size = sizeof(u8) * pixel_count * channels;
        state->default_bresource_cube_texture = create_default_bresource_texture(state, bname_create(DEFAULT_CUBE_TEXTURE_NAME), BRESOURCE_TEXTURE_TYPE_CUBE, tex_dimension, 6, channels, pixel_array_size, pixels);
        if (!state->default_bresource_cube_texture)
        {
            BERROR("Failed to request resources for default cube texture");
            return false;
        }
        bfree(pixels, image_size, MEMORY_TAG_ARRAY);
    }

    // Default terrain textures. 4 materials, 3 maps per, for 12 layers
    // TODO: This should be a default layered texture of n layers, if anything
    {
        u32 layer_size = sizeof(u8) * 16 * 16 * 4;
        u32 terrain_material_count = 4;
        u32 terrain_per_material_map_count = 3;
        u32 layer_count = terrain_per_material_map_count * terrain_material_count;
        u8* terrain_pixels = ballocate(layer_size * layer_count, MEMORY_TAG_ARRAY);
        u32 material_size = layer_size * terrain_per_material_map_count;
        for (u32 i = 0; i < terrain_material_count; ++i)
        {
            // Albedo NOTE: purposefully using checkerboard here instead of default diffuse white;
            bcopy_memory(terrain_pixels + (material_size * i) + (layer_size * 0), pixels, layer_size);
            // Normal
            bcopy_memory(terrain_pixels + (material_size * i) + (layer_size * 1), normal_pixels, layer_size);
            // Combined
            bcopy_memory(terrain_pixels + (material_size * i) + (layer_size * 2), combined_pixels, layer_size);
        }

        // Request new resource texture
        state->default_bresource_terrain_texture = create_default_bresource_texture(state, bname_create(DEFAULT_TERRAIN_TEXTURE_NAME), BRESOURCE_TEXTURE_TYPE_2D_ARRAY, tex_dimension, layer_count, channels, layer_size * layer_count, terrain_pixels);
        if (!state->default_bresource_terrain_texture)
        {
            BERROR("Failed to request resources for default terrain texture");
            return false;
        }
        bfree(terrain_pixels, layer_size * layer_count, MEMORY_TAG_ARRAY);
    }

    return true;
}

static void release_default_textures(texture_system_state* state)
{
    if (state)
    {
        bresource_system_release(state->bresource_system, state->default_bresource_texture->base.name);
        bresource_system_release(state->bresource_system, state->default_bresource_diffuse_texture->base.name);
        bresource_system_release(state->bresource_system, state->default_bresource_specular_texture->base.name);
        bresource_system_release(state->bresource_system, state->default_bresource_normal_texture->base.name);
        bresource_system_release(state->bresource_system, state->default_bresource_combined_texture->base.name);
        bresource_system_release(state->bresource_system, state->default_bresource_cube_texture->base.name);
        bresource_system_release(state->bresource_system, state->default_bresource_terrain_texture->base.name);
    }
}

static void increment_generation(bresource_texture* t)
{
    if (t)
    {
        t->base.generation++;
        // Ensure we don't land on invalid before rolling over
        if (t->base.generation == INVALID_ID_U8)
            t->base.generation = 0;
    }
}

static void invalidate_texture(bresource_texture* t)
{
    if (t)
    {
        bzero_memory(t, sizeof(bresource_texture));
        t->base.generation = INVALID_ID_U8;
        t->renderer_texture_handle = bhandle_invalid();
    }
}

static bresource_texture* default_texture_by_name(texture_system_state* state, bname name)
{
    if (name == state->default_bresource_texture->base.name) {
        return state->default_bresource_texture;
    } else if (name == state->default_bresource_diffuse_texture->base.name) {
        return state->default_bresource_diffuse_texture;
    } else if (name == state->default_bresource_normal_texture->base.name) {
        return state->default_bresource_normal_texture;
    } else if (name == state->default_bresource_specular_texture->base.name) {
        return state->default_bresource_specular_texture;
    } else if (name == state->default_bresource_combined_texture->base.name) {
        return state->default_bresource_combined_texture;
    } else if (name == state->default_bresource_cube_texture->base.name) {
        return state->default_bresource_cube_texture;
    } else if (name == state->default_bresource_terrain_texture->base.name) {
        return state->default_bresource_terrain_texture;
    }

    return 0;
}

static bresource_texture* request_writeable_arrayed(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency, bresource_texture_type type, u16 array_size, b8 is_depth, b8 multiframe_buffering)
{
    struct bresource_system_state* bresource_system = engine_systems_get()->bresource_state;
    bresource_texture_request_info request = {0};
    bzero_memory(&request, sizeof(bresource_texture_request_info));
    request.texture_type = type;
    request.array_size = array_size;
    request.flags = BRESOURCE_TEXTURE_FLAG_IS_WRITEABLE;
    request.flags |= has_transparency ? BRESOURCE_TEXTURE_FLAG_HAS_TRANSPARENCY : 0;
    request.flags |= is_depth ? BRESOURCE_TEXTURE_FLAG_DEPTH : 0;
    request.flags |= multiframe_buffering ? BRESOURCE_TEXTURE_FLAG_RENDERER_BUFFERING : 0;
    request.width = width;
    request.height = height;
    request.format = format;
    request.mip_levels = 1; // TODO: configurable?
    request.base.type = BRESOURCE_TYPE_TEXTURE;
    request.flip_y = false; // Doesn't really matter anyway for this type.
    bresource_texture* t = (bresource_texture*)bresource_system_request(bresource_system, name, (bresource_request_info*)&request);
    if (!t)
    {
        BERROR("Failed to request resources for arrayed writeable texture");
        return 0;
    }

    return t;
}
