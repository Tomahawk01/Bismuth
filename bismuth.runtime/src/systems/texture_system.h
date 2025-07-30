#pragma once

#include "bresources/bresource_types.h"

struct texture_system_state;

typedef struct texture_system_config
{
    u32 max_texture_count;
} texture_system_config;

#define DEFAULT_TEXTURE_NAME "Texture.Default"

#define DEFAULT_BASE_COLOR_TEXTURE_NAME "Texture.DefaultBase"
#define DEFAULT_SPECULAR_TEXTURE_NAME "Texture.DefaultSpecular"
#define DEFAULT_NORMAL_TEXTURE_NAME "Texture.DefaultNormal"
#define DEFAULT_MRA_TEXTURE_NAME "Texture.DefaultMRA"
#define DEFAULT_CUBE_TEXTURE_NAME "Texture.DefaultCube"
#define DEFAULT_WATER_NORMAL_TEXTURE_NAME "Texture.DefaultWaterNormal"
#define DEFAULT_WATER_DUDV_TEXTURE_NAME "Texture.DefaultWaterDUDV"

b8 texture_system_initialize(u64* memory_requirement, void* state, void* config);
void texture_system_shutdown(void* state);

typedef void (*PFN_texture_loaded_callback)(btexture* texture, void* listener);

BAPI btexture* texture_acquire(const char* image_asset_name, void* listener, PFN_texture_loaded_callback callback);
// auto_release=true, default options
BAPI btexture* texture_acquire_sync(const char* image_asset_name);

// auto_release=true, default options
BAPI btexture* texture_acquire_from_package(const char* image_asset_name, const char* package_name, void* listener, PFN_texture_loaded_callback callback);
BAPI btexture* texture_acquire_from_package_sync(const char* image_asset_name, const char* package_name);

BAPI btexture* texture_cubemap_acquire(const char* image_asset_name_prefix);
// auto_release=true, default options
BAPI btexture* texture_cubemap_acquire_from_package(const char* image_asset_name_prefix, const char* package_name);

/* // Easier idea? synchronous. auto_release=true, default options
BAPI btexture* texture_acquire_from_image(const struct basset_image* image);

BAPI btexture* texture_cubemap_acquire_from_images(const struct basset_image* images[6]); */

typedef struct btexture_load_options
{
    b8 is_writeable;
    b8 is_depth;
    b8 is_stencil;
    b8 multiframe_buffering;
    // Unload from GPU when reference count reaches 0
    b8 auto_release;
    bpixel_format format;
    btexture_type type;
    u32 width;
    u32 height;
    // Set to 0 to calculate mip levels based on size
    u8 mip_levels;
    union
    {
        u32 depth;
        u32 layer_count;
    };
    const char* name;
    // The name of the image asset to load for the texture. Optional. Only used for single-layer textures and cubemaps. Ignored for layered textures
    const char* image_asset_name;
    // The name of the image asset to load for the texture. Optional. Only used for single-layer textures and cubemaps. Ignored for layered textures
    const char* package_name;
    // Names of layer image assets, only used for array/layered textures. Element count must be layer_count
    const char** layer_image_asset_names;
    // Names of packages containing layer image assets, only used for array/layered textures. Element count must be layer_count. Use null/0 to load from application package
    const char** layer_package_names;

    // Block of pixel data, which can be multiple layers as defined by layer_count. The pixel data for all layers should be contiguous. Layout interpreted based on format
    void* pixel_data;
    // The size of the pixel_data array in bytes (NOT pixel count!)
    u32 pixel_array_size;
} btexture_load_options;

BAPI btexture* texture_acquire_with_options(btexture_load_options options, void* listener, PFN_texture_loaded_callback callback);
BAPI btexture* texture_acquire_with_options_sync(btexture_load_options options);

BAPI void texture_release(btexture* texture);

/* typedef struct bresource_texture_pixel_data
{
    u8* pixels;
    u32 pixel_array_size;
    u32 width;
    u32 height;
    u32 channel_count;
    texture_format format;
    u8 mip_levels;
} bresource_texture_pixel_data;

ARRAY_TYPE(bresource_texture_pixel_data);

typedef struct bresource_texture_request_info
{
    bresource_request_info base;

    texture_type texture_type;
    u8 array_size;
    texture_flag_bits flags;

    // Optionally provide pixel data per layer. Must match array_size in length.
    // Only used where asset at index has type of undefined
    array_bresource_texture_pixel_data pixel_data;

    // Texture width in pixels. Ignored unless there are no assets or pixel data
    u32 width;

    // Texture height in pixels. Ignored unless there are no assets or pixel data
    u32 height;

    // Texture format. Ignored unless there are no assets or pixel data
    texture_format format;

    // The number of mip levels. Ignored unless there are no assets or pixel data
    u8 mip_levels;

    // Indicates if loaded image assets should be flipped on the y-axis when loaded. Ignored for non-asset-based textures
    b8 flip_y;
} bresource_texture_request_info; */

BAPI bresource_texture* texture_system_request(bname name, bname package_name, void* listener, PFN_resource_loaded_user_callback callback);

BAPI bresource_texture* texture_system_request_cube(bname name, b8 auto_release, b8 multiframe_buffering, void* listener, PFN_resource_loaded_user_callback callback);
BAPI bresource_texture* texture_system_request_cube_writeable(bname name, u32 dimension, b8 auto_release, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_cube_depth(bname name, u32 dimension, b8 auto_release, b8 include_stencil, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_writeable(bname name, u32 width, u32 height, texture_format format, b8 has_transparency, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_writeable_arrayed(bname name, u32 width, u32 height, texture_format format, b8 has_transparency, b8 multiframe_buffering, texture_type type, u16 array_size);
BAPI bresource_texture* texture_system_request_depth(bname name, u32 width, u32 height, b8 include_stencil, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_depth_arrayed(bname name, u32 width, u32 height, u16 array_size, b8 include_stencil, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_acquire_textures_as_arrayed(bname name, bname package_name, u32 layer_count, bname* layer_asset_names, b8 auto_release, b8 multiframe_buffering, void* listener, PFN_resource_loaded_user_callback callback);
BAPI void texture_system_release_resource(bresource_texture* t);

BAPI b8 texture_system_resize(bresource_texture* t, u32 width, u32 height, b8 regenerate_internal_data);

BAPI b8 texture_system_write_data(bresource_texture* t, u32 offset, u32 size, void* data);
