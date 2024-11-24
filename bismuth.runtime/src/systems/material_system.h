#pragma once

#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "resources/resource_types.h"

#include <defines.h>
#include <strings/bname.h>

struct material_system_state;

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

typedef struct material_data
{
    // A unique id used for handle validation
    u64 unique_id;
    // Multiplied by albedo/diffuse texture. Default: 1,1,1,1 (white)
    vec4 albedo_color;
    bresource_texture* albedo_diffuse_texture;

    bresource_texture* normal_texture;

    f32 metallic;
    bresource_texture* metallic_texture;
    texture_channel metallic_texture_channel;

    f32 roughness;
    bresource_texture* roughness_texture;
    texture_channel roughness_texture_channel;

    bresource_texture* ao_texture;
    texture_channel ao_texture_channel;

    bresource_texture* emissive_texture;
    f32 emissive_texture_intensity;

    bresource_texture* refraction_texture;
    f32 refraction_scale;

    /**
     * @brief This is a combined texture holding metallic/roughness/ambient occlusion all in one texture.
     * This is a more efficient replacement for using those textures individually. Metallic is sampled
     * from the Red channel, roughness from the Green channel, and ambient occlusion from the Blue channel.
     * Alpha is ignored
     */
    bresource_texture* mra_texture;

    // Base set of flags for the material. Copied to the material instance when created
    material_flags flags;

    // Texture mode used for all textures on the material
    material_texture_mode texture_mode;

    // Texture filter used for all textures on the material
    material_texture_filter texture_filter;

    // Added to UV coords of vertex data. Overridden by instance data
    vec3 uv_offset;

    // Multiplied against uv coords of vertex data. Overridden by instance data
    vec3 uv_scale;

    // Shader group id for per-group uniforms
    u32 group_id;
} material_data;

typedef struct material_instance_data
{
    // A handle to the material to which this instance references
    bhandle material;
    // A unique id used for handle validation
    u64 unique_id;
    // Shader draw id for per-draw uniforms
    u32 per_draw_id;

    // Multiplied by albedo/diffuse texture. Overrides the value set in the base material
    vec4 albedo_color;

    // Overrides the flags set in the base material
    material_flags flags;

    // Added to UV coords of vertex data
    vec3 uv_offset;
    // Multiplied against uv coords of vertex data
    vec3 uv_scale;
} material_instance_data;

typedef struct multimaterial_data
{
    u8 submaterial_count;
    u16* material_ids;
} multimaterial_data;

typedef struct mulitmaterial_instance_data
{
    bhandle instance;
    u8 submaterial_count;
    material_instance_data* instance_datas;
} multimaterial_instance_data;

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

BAPI material_texture_filter material_texture_filter_get(struct material_system_state* state, bhandle material);
BAPI void material_texture_filter_set(struct material_system_state* state, bhandle material, material_texture_filter value);

BAPI material_texture_mode material_texture_mode_get(struct material_system_state* state, bhandle material);
BAPI void material_texture_mode_set(struct material_system_state* state, bhandle material, material_texture_mode value);

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

BAPI b8 material_flag_set(struct material_system_state* state, bhandle material, material_flag_bits flag, b8 value);
BAPI b8 material_flag_get(struct material_system_state* state, bhandle material, material_flag_bits flag);

// -------------------------------------------------
// ------------- MATERIAL INSTANCE -----------------
// -------------------------------------------------

BAPI bhandle material_acquire_instance(struct material_system_state* state, bhandle material);
BAPI void material_release_instance(struct material_system_state* state, bhandle material, bhandle instance);

BAPI b8 material_instance_flag_set(struct material_system_state* state, bhandle material, bhandle instance, material_flag_bits flag, b8 value);
b8 material_instance_flag_get(struct material_system_state* state, bhandle material, bhandle instance, material_flag_bits flag);

BAPI vec4 material_instance_albedo_color_get(struct material_system_state* state, bhandle material, bhandle instance);
BAPI void material_instance_albedo_color_set(struct material_system_state* state, bhandle material, bhandle instance, vec4 value);

BAPI vec3 material_instance_uv_offset_get(struct material_system_state* state, bhandle material, bhandle instance);
BAPI void material_instance_uv_offset_set(struct material_system_state* state, bhandle material, bhandle instance, vec3 value);

BAPI vec3 material_instance_uv_scale_get(struct material_system_state* state, bhandle material, bhandle instance);
BAPI void material_instance_uv_scale_set(struct material_system_state* state, bhandle material, bhandle instance, vec3 value);

BAPI bhandle material_get_default(struct material_system_state* state);

// Dumps all of the registered materials and their reference counts/handles
BAPI void material_system_dump(struct material_system_state* state);
