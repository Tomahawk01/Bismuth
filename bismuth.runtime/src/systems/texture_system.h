#pragma once

#include "core_render_types.h"
#include "bresources/bresource_types.h"

struct texture_system_state;

typedef struct texture_system_config
{
    u16 max_texture_count;
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

typedef void (*PFN_texture_loaded_callback)(btexture texture, void* listener);

BAPI btexture texture_acquire(const char* image_asset_name, void* listener, PFN_texture_loaded_callback callback);
BAPI btexture texture_acquire_sync(const char* image_asset_name);
BAPI void texture_release(btexture texture);

BAPI btexture texture_acquire_from_package(const char* image_asset_name, const char* package_name, void* listener, PFN_texture_loaded_callback callback);
BAPI btexture texture_acquire_from_package_sync(const char* image_asset_name, const char* package_name);

BAPI btexture texture_cubemap_acquire(const char* image_asset_name_prefix, void* listener, PFN_texture_loaded_callback callback);
BAPI btexture texture_cubemap_acquire_sync(const char* image_asset_name_prefix);
BAPI btexture texture_cubemap_acquire_from_package(const char* image_asset_name_prefix, const char* package_name, void* listener, PFN_texture_loaded_callback callback);

BAPI btexture texture_cubemap_acquire_from_package_sync(const char* image_asset_name_prefix, const char* package_name);

BAPI btexture texture_acquire_from_image(const struct kasset_image* image, const char* name);
BAPI btexture texture_acquire_from_pixel_data(bpixel_format format, u32 pixel_array_size, void* pixels, u32 width, u32 height, const char* name);
BAPI btexture texture_cubemap_acquire_from_pixel_data(bpixel_format format, u32 pixel_array_size, void* pixels, u32 width, u32 height, const char* name);

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

BAPI btexture texture_acquire_with_options(btexture_load_options options, void* listener, PFN_texture_loaded_callback callback);
BAPI btexture texture_acquire_with_options_sync(btexture_load_options options);

BAPI b8 texture_resize(btexture t, u32 width, u32 height, b8 regenerate_internal_data);

BAPI b8 texture_write_data(btexture t, u32 offset, u32 size, void* data);
