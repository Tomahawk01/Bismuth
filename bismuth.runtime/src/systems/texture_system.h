#pragma once

#include "bresources/bresource_types.h"
#include "renderer/renderer_types.h"

struct texture_system_state;

typedef struct texture_system_config
{
    u32 max_texture_count;
} texture_system_config;

#define DEFAULT_TEXTURE_NAME "default"
#define DEFAULT_DIFFUSE_TEXTURE_NAME "default_diff"
#define DEFAULT_SPECULAR_TEXTURE_NAME "default_spec"
#define DEFAULT_NORMAL_TEXTURE_NAME "default_norm"
#define DEFAULT_COMBINED_TEXTURE_NAME "default_combined"
#define DEFAULT_CUBE_TEXTURE_NAME "default_cube"
#define DEFAULT_TERRAIN_TEXTURE_NAME "default_terrain"

b8 texture_system_initialize(u64* memory_requirement, void* state, void* config);
void texture_system_shutdown(void* state);

BAPI bresource_texture* texture_system_request(bname name, bname package_name, void* listener, PFN_resource_loaded_user_callback callback);

BAPI texture* texture_system_acquire(const char* name, b8 auto_release);

BAPI bresource_texture* texture_system_request_cube(bname name, b8 auto_release, void* listener, PFN_resource_loaded_user_callback callback);
BAPI bresource_texture* texture_system_request_cube_writeable(bname name, u32 dimension, b8 auto_release);
BAPI bresource_texture* texture_system_request_cube_depth(bname name, u32 dimension, b8 auto_release);

BAPI bresource_texture* texture_system_request_writeable(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency);
BAPI bresource_texture* texture_system_request_writeable_arrayed(bname name, u32 width, u32 height, bresource_texture_format format, b8 has_transparency, bresource_texture_type type, u16 array_size);
BAPI bresource_texture* texture_system_request_depth(bname name, u32 width, u32 height);

BAPI bresource_texture* texture_system_request_depth_arrayed(bname name, u32 width, u32 height, u16 array_size);
BAPI bresource_texture* texture_system_acquire_textures_as_arrayed(bname name, bname package_name, u32 layer_count, bname* layer_asset_names, b8 auto_release, void* listener, PFN_resource_loaded_user_callback callback);
BAPI void texture_system_release(const char* name);
BAPI void texture_system_release_resource(bresource_texture* t);

BAPI void texture_system_wrap_internal(const char* name, u32 width, u32 height, u8 channel_count, b8 has_transparency, b8 is_writeable, b8 register_texture, b_handle renderer_texture_handle, texture* out_texture);
BAPI b8 texture_system_resize(bresource_texture* t, u32 width, u32 height, b8 regenerate_internal_data);
BAPI b8 texture_system_write_data(texture* t, u32 offset, u32 size, void* data);
BAPI b8 texture_system_is_default_texture(texture* t);

BAPI texture* texture_system_get_default_texture(void);
BAPI texture* texture_system_get_default_diffuse_texture(void);
BAPI texture* texture_system_get_default_specular_texture(void);
BAPI texture* texture_system_get_default_normal_texture(void);
BAPI texture* texture_system_get_default_combined_texture(void);
BAPI texture* texture_system_get_default_cube_texture(void);
BAPI texture* texture_system_get_default_terrain_texture(void);

BAPI const bresource_texture* texture_system_get_default_bresource_texture(struct texture_system_state* state);
BAPI const bresource_texture* texture_system_get_default_bresource_diffuse_texture(struct texture_system_state* state);
BAPI const bresource_texture* texture_system_get_default_bresource_specular_texture(struct texture_system_state* state);
BAPI const bresource_texture* texture_system_get_default_bresource_normal_texture(struct texture_system_state* state);
BAPI const bresource_texture* texture_system_get_default_bresource_combined_texture(struct texture_system_state* state);
BAPI const bresource_texture* texture_system_get_default_bresource_cube_texture(struct texture_system_state* state);
BAPI const bresource_texture* texture_system_get_default_bresource_terrain_texture(struct texture_system_state* state);

BAPI struct texture_internal_data* texture_system_get_internal_or_default(texture* t, u8* out_generation);
BAPI struct texture_internal_data* texture_system_resource_get_internal_or_default(const bresource_texture* t, u32* out_generation);
