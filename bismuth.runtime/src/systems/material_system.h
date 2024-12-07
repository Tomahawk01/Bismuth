#pragma once

#include "core_render_types.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"

#include <defines.h>
#include <strings/bname.h>

#define MATERIAL_DEFAULT_NAME_STANDARD "Material.DefaultStandard"
#define MATERIAL_DEFAULT_NAME_WATER "Material.DefaultWater"
#define MATERIAL_DEFAULT_NAME_BLENDED "Material.DefaultBlended"

struct material_system_state;
struct frame_data;

typedef struct material_system_config
{
    u32 max_material_count;

    u32 max_instance_count;
} material_system_config;

typedef enum material_texture_param
{
    // Albedo for PBR, sometimes known as a "diffuse" color. Specifies per-pixel color
    MATERIAL_TEXTURE_PARAM_ALBEDO = 0,
    // Texture specifying per-pixel normal vector
    MATERIAL_TEXTURE_PARAM_NORMAL = 1,
    // Texture specifying per-pixel metallic value
    MATERIAL_TEXTURE_PARAM_METALLIC = 2,
    // Texture specifying per-pixel roughness value
    MATERIAL_TEXTURE_PARAM_ROUGHNESS = 3,
    // Texture specifying per-pixel ambient occlusion value
    MATERIAL_TEXTURE_PARAM_AMBIENT_OCCLUSION = 4,
    // Texture specifying per-pixel emissive value
    MATERIAL_TEXTURE_PARAM_EMISSIVE = 5,
    // Texture specifying per-pixel refraction strength
    MATERIAL_TEXTURE_INPUT_REFRACTION = 6,
    // Texture holding per-pixel metallic (r), roughness (g) and ambient occlusion (b) value
    MATERIAL_TEXTURE_MRA = 7,
    // The size of the material_texture_param enumeration
    MATERIAL_TEXTURE_COUNT
} material_texture_param;

/**
 * @brief A material instance, which contains handles to both
 * the base material as well as the instance itself. Every time
 * an instance is "acquired", one of these is created, and the instance
 * should be referenced using this going from that point
 */
typedef struct material_instance
{
    // Handle to the base material
    bhandle material;
    // Handle to the instance
    bhandle instance;
} material_instance;

b8 material_system_initialize(u64* memory_requirement, struct material_system_state* state, const material_system_config* config);
void material_system_shutdown(struct material_system_state* state);

// -------------------------------------------------
// ---------------- MATERIAL -----------------------
// -------------------------------------------------

BAPI b8 material_system_get_handle(struct material_system_state* state, bname name, bhandle* out_material_handle);

BAPI const bresource_texture* material_texture_get(struct material_system_state* state, bhandle material, material_texture_param tex_param);
BAPI void material_texture_set(struct material_system_state* state, bhandle material, material_texture_param tex_param, const bresource_texture* texture);

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

BAPI b8 material_use_vertex_color_as_albedo_get(struct material_system_state* state, bhandle material);
BAPI void material_use_vertex_color_as_albedo_set(struct material_system_state* state, bhandle material, b8 value);

BAPI b8 material_flag_set(struct material_system_state* state, bhandle material, bmaterial_flag_bits flag, b8 value);
BAPI b8 material_flag_get(struct material_system_state* state, bhandle material, bmaterial_flag_bits flag);

// -------------------------------------------------
// ------------- MATERIAL INSTANCE -----------------
// -------------------------------------------------

BAPI b8 material_system_acquire(struct material_system_state* state, bname name, material_instance* out_instance);
BAPI void material_system_release(struct material_system_state* state, material_instance* instance);

BAPI b8 material_system_prepare_frame(struct material_system_state* state, struct frame_data* p_frame_data);
BAPI b8 material_system_apply(struct material_system_state* state, bhandle material, struct frame_data* p_frame_data);
BAPI b8 material_system_apply_instance(struct material_system_state* state, const material_instance* instance, struct frame_data* p_frame_data);

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
