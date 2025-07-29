#pragma once

#include "core_render_types.h"
#include <assets/basset_types.h>
#include <defines.h>
#include <math/math_types.h>
#include <resources/resource_types.h>

typedef struct obj_mtl_source_material
{
    // Name of the material
    bname name;
    // Material type
    bmaterial_type type;
    // Material lighting model
    bmaterial_model model;

    vec3 ambient_color;
    bname ambient_image_asset_name;
    vec3 diffuse_color;
    bname diffuse_image_asset_name;

    f32 diffuse_transparency;
    bname diffuse_transparency_image_asset_name;

    f32 roughness;
    bname roughness_image_asset_name;
    f32 metallic;
    bname metallic_image_asset_name;
    f32 sheen;
    bname sheen_image_asset_name;

    bname normal_image_asset_name;

    bname displacement_image_asset_name;

    vec3 specular_color;
    f32 specular_exponent;
    bname specular_image_asset_name;

    // 0-1, 1=fully opaque, 0=fully transparent
    f32 transparency;
    bname alpha_image_asset_name;

    bname mra_image_asset_name;

    vec3 emissive_color;
    bname emissive_image_asset_name;
    
    // Index of refraction/optical density. [0.001-10]. 1.0 means light does not bend as it passes through. Glass should be ~1.5. Values < 1.0 are strange
    f32 ior;
} obj_mtl_source_material;

typedef struct obj_mtl_source_asset
{
    u32 material_count;
    obj_mtl_source_material* materials;
} obj_mtl_source_asset;

BAPI b8 obj_mtl_serializer_serialize(const obj_mtl_source_asset* source_asset, const char** out_file_text);
BAPI b8 obj_mtl_serializer_deserialize(const char* mtl_file_text, obj_mtl_source_asset* out_mtl_source_asset);
