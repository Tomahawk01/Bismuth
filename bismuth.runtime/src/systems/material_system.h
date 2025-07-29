#pragma once

#include "core_render_types.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"

#include <defines.h>
#include <strings/bname.h>

#define MATERIAL_DEFAULT_NAME_STANDARD "Material.DefaultStandard"
#define MATERIAL_DEFAULT_NAME_WATER "Material.DefaultWater"
#define MATERIAL_DEFAULT_NAME_BLENDED "Material.DefaultBlended"

#define MATERIAL_MAX_IRRADIANCE_CUBEMAP_COUNT 4
#define MATERIAL_MAX_SHADOW_CASCADES 4
#define MATERIAL_MAX_POINT_LIGHTS 10
#define MATERIAL_MAX_VIEWS 4

#define MATERIAL_DEFAULT_BASE_COLOR_VALUE (vec4){1.0f, 1.0f, 1.0f, 1.0f}
#define MATERIAL_DEFAULT_NORMAL_VALUE (vec3){0.0f, 0.0f, 1.0f}
#define MATERIAL_DEFAULT_NORMAL_ENABLED true
#define MATERIAL_DEFAULT_METALLIC_VALUE 0.0f
#define MATERIAL_DEFAULT_ROUGHNESS_VALUE 0.5f
#define MATERIAL_DEFAULT_AO_VALUE 1.0f
#define MATERIAL_DEFAULT_AO_ENABLED true
#define MATERIAL_DEFAULT_MRA_VALUE (vec3){0.0f, 0.5f, 1.0f}
#define MATERIAL_DEFAULT_MRA_ENABLED true
#define MATERIAL_DEFAULT_HAS_TRANSPARENCY false
#define MATERIAL_DEFAULT_DOUBLE_SIDED false
#define MATERIAL_DEFAULT_RECIEVES_SHADOW true
#define MATERIAL_DEFAULT_CASTS_SHADOW true
#define MATERIAL_DEFAULT_USE_VERTEX_COLOR_AS_BASE_COLOR false

struct material_system_state;
struct frame_data;

typedef struct material_system_config
{
    u32 max_material_count;

    u32 max_instance_count;
} material_system_config;

typedef enum material_texture_input
{
    // Forms the base color of a material. Albedo for PBR, sometimes known as a "diffuse" color. Specifies per-pixel color
    MATERIAL_TEXTURE_INPUT_BASE_COLOR,
    // Texture specifying per-pixel normal vector
    MATERIAL_TEXTURE_INPUT_NORMAL,
    // Texture specifying per-pixel metallic value
    MATERIAL_TEXTURE_INPUT_METALLIC,
    // Texture specifying per-pixel roughness value
    MATERIAL_TEXTURE_INPUT_ROUGHNESS,
    // Texture specifying per-pixel ambient occlusion value
    MATERIAL_TEXTURE_INPUT_AMBIENT_OCCLUSION,
    // Texture specifying per-pixel emissive value
    MATERIAL_TEXTURE_INPUT_EMISSIVE,
    // Texture specifying the reflection (only used for water materials)
    MATERIAL_TEXTURE_INPUT_REFLECTION,
    // Texture specifying per-pixel refraction strength
    MATERIAL_TEXTURE_INPUT_REFRACTION,
    // Texture specifying the reflection depth (only used for water materials)
    MATERIAL_TEXTURE_INPUT_REFLECTION_DEPTH,
    // Texture specifying the refraction depth
    MATERIAL_TEXTURE_INPUT_REFRACTION_DEPTH,
    MATERIAL_TEXTURE_INPUT_DUDV,
    // Texture holding per-pixel metallic (r), roughness (g) and ambient occlusion (b) value
    MATERIAL_TEXTURE_INPUT_MRA,
    // The size of the material_texture_input enumeration
    MATERIAL_TEXTURE_INPUT_COUNT
} material_texture_input;

b8 material_system_initialize(u64* memory_requirement, struct material_system_state* state, const material_system_config* config);
void material_system_shutdown(struct material_system_state* state);

// -------------------------------------------------
// ---------------- MATERIAL -----------------------
// -------------------------------------------------

BAPI b8 material_system_get_handle(struct material_system_state* state, bname name, bhandle* out_material_handle);

BAPI bresource_texture* material_texture_get(struct material_system_state* state, bhandle material, material_texture_input tex_input);
BAPI void material_texture_set(struct material_system_state* state, bhandle material, material_texture_input tex_input, bresource_texture* texture);

BAPI texture_channel material_metallic_texture_channel_get(struct material_system_state* state, bhandle material);
BAPI void material_metallic_texture_channel_set(struct material_system_state* state, bhandle material, texture_channel value);

BAPI texture_channel material_roughness_texture_channel_get(struct material_system_state* state, bhandle material);
BAPI void material_roughness_texture_channel_set(struct material_system_state* state, bhandle material, texture_channel value);

BAPI texture_channel material_ao_texture_channel_get(struct material_system_state* state, bhandle material);
BAPI void material_ao_texture_channel_set(struct material_system_state* state, bhandle material, texture_channel value);

BAPI texture_filter material_texture_filter_get(struct material_system_state* state, bhandle material);
BAPI void material_texture_filter_set(struct material_system_state* state, bhandle material, texture_filter value);

BAPI texture_repeat material_texture_mode_get(struct material_system_state* state, bhandle material);
BAPI void material_texture_mode_set(struct material_system_state* state, bhandle material, texture_repeat value);

BAPI b8 material_has_transparency_get(struct material_system_state* state, bhandle material);
BAPI void material_has_transparency_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_double_sided_get(struct material_system_state* state, bhandle material);
BAPI void material_double_sided_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_recieves_shadow_get(struct material_system_state* state, bhandle material);
BAPI void material_recieves_shadow_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_casts_shadow_get(struct material_system_state* state, bhandle material);
BAPI void material_casts_shadow_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_normal_enabled_get(struct material_system_state* state, bhandle material);
BAPI void material_normal_enabled_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_ao_enabled_get(struct material_system_state* state, bhandle material);
BAPI void material_ao_enabled_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_emissive_enabled_get(struct material_system_state* state, bhandle material);
BAPI void material_emissive_enabled_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_refraction_enabled_get(struct material_system_state* state, bhandle material);
BAPI void material_refraction_enabled_set(struct material_system_state* state, bhandle material, b8 value);

BAPI f32 material_refraction_scale_get(struct material_system_state* state, bhandle material);
BAPI void material_refraction_scale_set(struct material_system_state* state, bhandle material, f32 value);

BAPI b8 material_use_vertex_color_as_base_color_get(struct material_system_state* state, bhandle material);
BAPI void material_use_vertex_color_as_base_color_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_flag_set(struct material_system_state* state, bhandle material, bmaterial_flag_bits flag, b8 value);
BAPI b8 material_flag_get(struct material_system_state* state, bhandle material, bmaterial_flag_bits flag);

// -------------------------------------------------
// ------------- MATERIAL INSTANCE -----------------
// -------------------------------------------------

BAPI b8 material_system_acquire(struct material_system_state* state, bname name, material_instance* out_instance);
BAPI void material_system_release(struct material_system_state* state, material_instance* instance);

/** @brief Holds internal state for per-frame data (i.e across all standard materials); */
typedef struct material_frame_data
{
    // Light space for shadow mapping. Per cascade
    mat4 directional_light_spaces[MATERIAL_MAX_SHADOW_CASCADES];
    mat4 projection;
    mat4 views[MATERIAL_MAX_VIEWS];
    vec4 view_positions[MATERIAL_MAX_VIEWS];
    u32 render_mode;
    f32 cascade_splits[MATERIAL_MAX_SHADOW_CASCADES];
    f32 shadow_bias;
    f32 delta_time;
    f32 game_time;
    bresource_texture* shadow_map_texture;
    bresource_texture* irradiance_cubemap_textures[MATERIAL_MAX_SHADOW_CASCADES];
} material_frame_data;
b8 material_system_prepare_frame(struct material_system_state* state, material_frame_data mat_frame_data, struct frame_data* p_frame_data);

BAPI b8 material_system_apply(struct material_system_state* state, bhandle material, struct frame_data* p_frame_data);

typedef struct material_instance_draw_data
{
    mat4 model;
    vec4 clipping_plane;
    u32 irradiance_cubemap_index;
    u32 view_index;
} material_instance_draw_data;
b8 material_system_apply_instance(struct material_system_state* state, const material_instance* instance, struct material_instance_draw_data draw_data, struct frame_data* p_frame_data);

BAPI b8 material_instance_flag_set(struct material_system_state* state, material_instance instance, bmaterial_flag_bits flag, b8 value);
BAPI b8 material_instance_flag_get(struct material_system_state* state, material_instance instance, bmaterial_flag_bits flag);

BAPI b8 material_instance_base_color_get(struct material_system_state* state, material_instance instance, vec4* out_value);
BAPI b8 material_instance_base_color_set(struct material_system_state* state, material_instance instance, vec4 value);

BAPI b8 material_instance_uv_offset_get(struct material_system_state* state, material_instance instance, vec3* out_value);
BAPI b8 material_instance_uv_offset_set(struct material_system_state* state, material_instance instance, vec3 value);

BAPI b8 material_instance_uv_scale_get(struct material_system_state* state, material_instance instance, vec3* out_value);
BAPI b8 material_instance_uv_scale_set(struct material_system_state* state, material_instance instance, vec3 value);

BAPI material_instance material_system_get_default_standard(struct material_system_state* state);
BAPI material_instance material_system_get_default_water(struct material_system_state* state);
BAPI material_instance material_system_get_default_blended(struct material_system_state* state);

// Dumps all of the registered materials and their reference counts/handles
BAPI void material_system_dump(struct material_system_state* state);
