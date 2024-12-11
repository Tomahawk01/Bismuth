#pragma once

#include "core_render_types.h"
#include <assets/basset_types.h>
#include <defines.h>
#include <math/math_types.h>
#include <resources/resource_types.h>

typedef enum obj_texture_map_channel
{
    OBJ_TEXTURE_MAP_CHANNEL_PBR_ALBEDO,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_NORMAL,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_METALLIC,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_ROUGHNESS,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_AO,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_EMISSIVE,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_CLEAR_COAT,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_CLEAR_COAT_ROUGHNESS,
    OBJ_TEXTURE_MAP_CHANNEL_PBR_WATER,
    OBJ_TEXTURE_MAP_CHANNEL_PHONG_DIFFUSE,
    OBJ_TEXTURE_MAP_CHANNEL_PHONG_NORMAL,
    OBJ_TEXTURE_MAP_CHANNEL_PHONG_SPECULAR,
    OBJ_TEXTURE_MAP_CHANNEL_UNLIT_COLOR
} obj_texture_map_channel;

typedef struct obj_mtl_source_texture_map
{
    // Name of the texture map
    bname name;
    // Name of the image asset
    bname image_asset_name;
    // The texture channel to be used
    obj_texture_map_channel channel;
} obj_mtl_source_texture_map;

typedef struct obj_mtl_source_property
{
    bname name;
    shader_uniform_type type;
    u32 size;
    union
    {
        vec4 v4;
        vec3 v3;
        vec2 v2;
        f32 f32;
        u32 u32;
        u16 u16;
        u8 u8;
        i32 i32;
        i16 i16;
        i8 i8;
        mat4 mat4;
    } value;
} obj_mtl_source_property;

typedef struct obj_mtl_source_material
{
    // Name of the material
    bname name;
    // Material type
    bmaterial_type type;
    // Material lighting model
    bmaterial_model model;
    // Texture maps
    u32 texture_map_count;
    obj_mtl_source_texture_map* maps;

    u32 property_count;
    obj_mtl_source_property* properties;
} obj_mtl_source_material;

typedef struct obj_mtl_source_asset
{
    u32 material_count;
    obj_mtl_source_material* materials;
} obj_mtl_source_asset;

BAPI b8 obj_mtl_serializer_serialize(const obj_mtl_source_asset* source_asset, const char** out_file_text);
BAPI b8 obj_mtl_serializer_deserialize(const char* mtl_file_text, obj_mtl_source_asset* out_mtl_source_asset);
