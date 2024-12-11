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

BAPI bresource_texture* texture_system_request(bname name, bname package_name, void* listener, PFN_resource_loaded_user_callback callback);

BAPI bresource_texture* texture_system_request_cube(bname name, b8 auto_release, b8 multiframe_buffering, void* listener, PFN_resource_loaded_user_callback callback);
BAPI bresource_texture* texture_system_request_cube_writeable(bname name, u32 dimension, b8 auto_release, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_cube_depth(bname name, u32 dimension, b8 auto_release, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_writeable(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_writeable_arrayed(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency, b8 multiframe_buffering, bresource_texture_type type, u16 array_size);
BAPI bresource_texture* texture_system_request_depth(bname name, u32 width, u32 height, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_request_depth_arrayed(bname name, u32 width, u32 height, u16 array_size, b8 multiframe_buffering);
BAPI bresource_texture* texture_system_acquire_textures_as_arrayed(bname name, bname package_name, u32 layer_count, bname* layer_asset_names, b8 auto_release, b8 multiframe_buffering, void* listener, PFN_resource_loaded_user_callback callback);
BAPI void texture_system_release_resource(bresource_texture* t);

BAPI b8 texture_system_resize(bresource_texture* t, u32 width, u32 height, b8 regenerate_internal_data);

BAPI b8 texture_system_write_data(bresource_texture* t, u32 offset, u32 size, void* data);
