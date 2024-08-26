#include "render_type_utils.h"

#include "assets/basset_types.h"
#include "debug/bassert.h"
#include "strings/bstring.h"

const char* texture_repeat_to_string(texture_repeat repeat)
{
    switch (repeat)
    {
    case TEXTURE_REPEAT_REPEAT:
        return "repeat";
    case TEXTURE_REPEAT_CLAMP_TO_EDGE:
        return "clamp_to_edge";
    case TEXTURE_REPEAT_CLAMP_TO_BORDER:
        return "clamp_to_border";
    case TEXTURE_REPEAT_MIRRORED_REPEAT:
        return "mirrored_repeat";
    default:
        BASSERT_MSG(false, "Unrecognized texture repeat");
        return 0;
    }
}

const char* texture_filter_mode_to_string(texture_filter filter)
{
    switch (filter)
    {
    case TEXTURE_FILTER_MODE_LINEAR:
        return "linear";
    case TEXTURE_FILTER_MODE_NEAREST:
        return "nearest";
    default:
        BASSERT_MSG(false, "Unrecognized texture filter type");
        return 0;
    }
}

const char* shader_uniform_type_to_string(shader_uniform_type type)
{
    switch (type)
    {
    case SHADER_UNIFORM_TYPE_FLOAT32:
        return "f32";
    case SHADER_UNIFORM_TYPE_FLOAT32_2:
        return "vec2";
    case SHADER_UNIFORM_TYPE_FLOAT32_3:
        return "vec3";
    case SHADER_UNIFORM_TYPE_FLOAT32_4:
        return "vec4";
    case SHADER_UNIFORM_TYPE_INT8:
        return "i8";
    case SHADER_UNIFORM_TYPE_INT16:
        return "i16";
    case SHADER_UNIFORM_TYPE_INT32:
        return "i32";
    case SHADER_UNIFORM_TYPE_UINT8:
        return "u8";
    case SHADER_UNIFORM_TYPE_UINT16:
        return "u16";
    case SHADER_UNIFORM_TYPE_UINT32:
        return "u32";
    case SHADER_UNIFORM_TYPE_MATRIX_4:
        return "mat4";
    case SHADER_UNIFORM_TYPE_SAMPLER_1D:
        return "sampler1d";
    case SHADER_UNIFORM_TYPE_SAMPLER_2D:
        return "sampler2d";
    case SHADER_UNIFORM_TYPE_SAMPLER_3D:
        return "sampler3d";
    case SHADER_UNIFORM_TYPE_SAMPLER_1D_ARRAY:
        return "sampler1dArray";
    case SHADER_UNIFORM_TYPE_SAMPLER_2D_ARRAY:
        return "sampler2dArray";
    case SHADER_UNIFORM_TYPE_SAMPLER_CUBE:
        return "samplerCube";
    case SHADER_UNIFORM_TYPE_SAMPLER_CUBE_ARRAY:
        return "samplerCubeArray";
    case SHADER_UNIFORM_TYPE_CUSTOM:
        return "custom";
    default:
        BASSERT_MSG(false, "Unrecognized uniform type");
        return 0;
    }
}

texture_repeat string_to_texture_repeat(const char* str)
{
    if (strings_equali("repeat", str))
    {
        return TEXTURE_REPEAT_REPEAT;
    }
    else if (strings_equali("clamp_to_edge", str))
    {
        return TEXTURE_REPEAT_CLAMP_TO_EDGE;
    }
    else if (strings_equali("clamp_to_border", str))
    {
        return TEXTURE_REPEAT_CLAMP_TO_BORDER;
    }
    else if (strings_equali("mirrored_repeat", str))
    {
        return TEXTURE_REPEAT_MIRRORED_REPEAT;
    }
    else
    {
        BASSERT_MSG(false, "Unrecognized texture repeat");
        return TEXTURE_REPEAT_REPEAT;
    }
}

texture_filter string_to_texture_filter_mode(const char* str)
{
    if (strings_equali("linear", str))
    {
        return TEXTURE_FILTER_MODE_LINEAR;
    }
    else if (strings_equali("nearest", str))
    {
        return TEXTURE_FILTER_MODE_LINEAR;
    }
    else
    {
        BASSERT_MSG(false, "Unrecognized texture filter type");
        return TEXTURE_FILTER_MODE_LINEAR;
    }
}

shader_uniform_type string_to_shader_uniform_type(const char* str)
{
    if (strings_equali("f32", str)) {
        return SHADER_UNIFORM_TYPE_FLOAT32;
    }
    else if (strings_equali("vec2", str)) {
        return SHADER_UNIFORM_TYPE_FLOAT32_2;
    }
    else if (strings_equali("vec3", str)) {
        return SHADER_UNIFORM_TYPE_FLOAT32_3;
    }
    else if (strings_equali("vec4", str)) {
        return SHADER_UNIFORM_TYPE_FLOAT32_4;
    }
    else if (strings_equali("i8", str)) {
        return SHADER_UNIFORM_TYPE_INT8;
    }
    else if (strings_equali("i16", str)) {
        return SHADER_UNIFORM_TYPE_INT16;
    }
    else if (strings_equali("i32", str)) {
        return SHADER_UNIFORM_TYPE_INT32;
    }
    else if (strings_equali("u8", str)) {
        return SHADER_UNIFORM_TYPE_UINT8;
    }
    else if (strings_equali("u16", str)) {
        return SHADER_UNIFORM_TYPE_UINT16;
    }
    else if (strings_equali("u32", str)) {
        return SHADER_UNIFORM_TYPE_UINT32;
    }
    else if (strings_equali("mat4", str)) {
        return SHADER_UNIFORM_TYPE_MATRIX_4;
    }
    else if (strings_equali("sampler1d", str)) {
        return SHADER_UNIFORM_TYPE_SAMPLER_1D;
    }
    else if (strings_equali("sampler2d", str)) {
        return SHADER_UNIFORM_TYPE_SAMPLER_2D;
    }
    else if (strings_equali("sampler3d", str)) {
        return SHADER_UNIFORM_TYPE_SAMPLER_3D;
    }
    else if (strings_equali("sampler1dArray", str)) {
        return SHADER_UNIFORM_TYPE_SAMPLER_1D_ARRAY;
    }
    else if (strings_equali("sampler2dArray", str)) {
        return SHADER_UNIFORM_TYPE_SAMPLER_2D_ARRAY;
    }
    else if (strings_equali("samplerCube", str)) {
        return SHADER_UNIFORM_TYPE_SAMPLER_CUBE;
    }
    else if (strings_equali("samplerCubeArray", str)) {
        return SHADER_UNIFORM_TYPE_SAMPLER_CUBE_ARRAY;
    }
    else if (strings_equali("custom", str)) {
        return SHADER_UNIFORM_TYPE_CUSTOM;
    }
    else
    {
        BASSERT_MSG(false, "Unrecognized uniform type");
        return SHADER_UNIFORM_TYPE_FLOAT32;
    }
}

const char* bmaterial_type_to_string(bmaterial_type type)
{
    switch (type)
    {
    case BMATERIAL_TYPE_PBR:
        return "pbr";
    case BMATERIAL_TYPE_PBR_WATER:
        return "pbr_water";
    case BMATERIAL_TYPE_PBR_TERRAIN:
        return "pbr_terrain";
    case BMATERIAL_TYPE_UNLIT:
        return "unlit";
    case BMATERIAL_TYPE_CUSTOM:
        return "custom";
    default:
        BASSERT_MSG(false, "Unrecognized material type");
        return "unlit";
    }
}

bmaterial_type string_to_bmaterial_type(const char* str)
{
    if (strings_equali(str, "pbr")) {
        return BMATERIAL_TYPE_PBR;
    }
    else if (strings_equali(str, "pbr_water")) {
        return BMATERIAL_TYPE_PBR_WATER;
    }
    else if (strings_equali(str, "pbr_terrain")) {
        return BMATERIAL_TYPE_PBR_TERRAIN;
    }
    else if (strings_equali(str, "unlit")) {
        return BMATERIAL_TYPE_UNLIT;
    }
    else if (strings_equali(str, "custom")) {
        return BMATERIAL_TYPE_CUSTOM;
    }
    else
    {
        BASSERT_MSG(false, "Unrecognized material type");
        return BMATERIAL_TYPE_UNLIT;
    }
}

const char* material_map_channel_to_string(basset_material_map_channel channel)
{
    switch (channel)
    {
    case BASSET_MATERIAL_MAP_CHANNEL_NORMAL:
        return "normal";
    case BASSET_MATERIAL_MAP_CHANNEL_ALBEDO:
        return "albedo";
    case BASSET_MATERIAL_MAP_CHANNEL_METALLIC:
        return "metallic";
    case BASSET_MATERIAL_MAP_CHANNEL_ROUGHNESS:
        return "roughness";
    case BASSET_MATERIAL_MAP_CHANNEL_AO:
        return "ao";
    case BASSET_MATERIAL_MAP_CHANNEL_EMISSIVE:
        return "emissive";
    case BASSET_MATERIAL_MAP_CHANNEL_CLEAR_COAT:
        return "clearcoat";
    case BASSET_MATERIAL_MAP_CHANNEL_CLEAR_COAT_ROUGHNESS:
        return "clearcoat_roughness";
    case BASSET_MATERIAL_MAP_CHANNEL_WATER_DUDV:
        return "dudv";
    case BASSET_MATERIAL_MAP_CHANNEL_DIFFUSE:
        return "diffuse";
    case BASSET_MATERIAL_MAP_CHANNEL_SPECULAR:
        return "specular";
    default:
        BASSERT_MSG(false, "map channel not supported for material type");
        return 0;
    }
}

basset_material_map_channel string_to_material_map_channel(const char* str)
{
    if (strings_equali(str, "albedo")) {
        return BASSET_MATERIAL_MAP_CHANNEL_ALBEDO;
    }
    else if (strings_equali(str, "normal")) {
        return BASSET_MATERIAL_MAP_CHANNEL_NORMAL;
    }
    else if (strings_equali(str, "metallic")) {
        return BASSET_MATERIAL_MAP_CHANNEL_METALLIC;
    }
    else if (strings_equali(str, "roughness")) {
        return BASSET_MATERIAL_MAP_CHANNEL_ROUGHNESS;
    }
    else if (strings_equali(str, "ao")) {
        return BASSET_MATERIAL_MAP_CHANNEL_AO;
    }
    else if (strings_equali(str, "emissive")) {
        return BASSET_MATERIAL_MAP_CHANNEL_EMISSIVE;
    }
    else if (strings_equali(str, "clearcoat")) {
        return BASSET_MATERIAL_MAP_CHANNEL_CLEAR_COAT;
    }
    else if (strings_equali(str, "clearcoat_roughness")) {
        return BASSET_MATERIAL_MAP_CHANNEL_CLEAR_COAT_ROUGHNESS;
    }
    else if (strings_equali(str, "dudv")) {
        return BASSET_MATERIAL_MAP_CHANNEL_WATER_DUDV;
    }
    else if (strings_equali(str, "diffuse")) {
        return BASSET_MATERIAL_MAP_CHANNEL_DIFFUSE;
    }
    else if (strings_equali(str, "specular")) {
        return BASSET_MATERIAL_MAP_CHANNEL_SPECULAR;
    }
    else
    {
        BASSERT_MSG(false, "map channel not supported for material type");
        return BASSET_MATERIAL_MAP_CHANNEL_DIFFUSE;
    }
}
