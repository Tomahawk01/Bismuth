#include "basset_bresource_utils.h"
#include "bresources/bresource_types.h"
#include "logger.h"

bresource_material_type basset_material_type_to_bresource(basset_material_type type)
{
    switch (type)
    {
    case BASSET_MATERIAL_TYPE_UNKNOWN:
        return BRESOURCE_MATERIAL_TYPE_UNKNOWN;
    case BASSET_MATERIAL_TYPE_STANDARD:
        return BRESOURCE_MATERIAL_TYPE_STANDARD;
    case BASSET_MATERIAL_TYPE_WATER:
        return BRESOURCE_MATERIAL_TYPE_WATER;
    case BASSET_MATERIAL_TYPE_BLENDED:
        return BRESOURCE_MATERIAL_TYPE_BLENDED;
    case BASSET_MATERIAL_TYPE_COUNT:
        BWARN("basset_material_type_to_bresource converting count - did you mean to do this?");
        return BRESOURCE_MATERIAL_TYPE_COUNT;
    case BASSET_MATERIAL_TYPE_CUSTOM:
        return BRESOURCE_MATERIAL_TYPE_CUSTOM;
    }
}

basset_material_type bresource_material_type_to_basset(bresource_material_type type)
{
    switch (type)
    {
    case BRESOURCE_MATERIAL_TYPE_UNKNOWN:
        return BASSET_MATERIAL_TYPE_UNKNOWN;
    case BRESOURCE_MATERIAL_TYPE_STANDARD:
        return BASSET_MATERIAL_TYPE_STANDARD;
    case BRESOURCE_MATERIAL_TYPE_WATER:
        return BASSET_MATERIAL_TYPE_WATER;
    case BRESOURCE_MATERIAL_TYPE_BLENDED:
        return BASSET_MATERIAL_TYPE_BLENDED;
    case BRESOURCE_MATERIAL_TYPE_COUNT:
        return BASSET_MATERIAL_TYPE_COUNT;
        BWARN("bresource_material_type_to_basset converting count - did you mean to do this?");
    case BRESOURCE_MATERIAL_TYPE_CUSTOM:
        return BASSET_MATERIAL_TYPE_CUSTOM;
    }
}

bresource_material_model basset_material_model_to_bresource(basset_material_model model)
{
    switch (model)
    {
    case BASSET_MATERIAL_MODEL_UNLIT:
        return BRESOURCE_MATERIAL_MODEL_UNLIT;
    case BASSET_MATERIAL_MODEL_PBR:
        return BRESOURCE_MATERIAL_MODEL_PBR;
    case BASSET_MATERIAL_MODEL_PHONG:
        return BRESOURCE_MATERIAL_MODEL_PHONG;
    case BASSET_MATERIAL_MODEL_COUNT:
        BWARN("basset_material_model_to_bresource converting count - did you mean to do this?");
        return BRESOURCE_MATERIAL_MODEL_COUNT;
    case BASSET_MATERIAL_MODEL_CUSTOM:
        return BRESOURCE_MATERIAL_MODEL_CUSTOM;
    }
}

basset_material_model bresource_material_model_to_basset(bresource_material_model model)
{
    switch (model)
    {
    case BRESOURCE_MATERIAL_MODEL_UNLIT:
        return BASSET_MATERIAL_MODEL_UNLIT;
    case BRESOURCE_MATERIAL_MODEL_PBR:
        return BASSET_MATERIAL_MODEL_PBR;
    case BRESOURCE_MATERIAL_MODEL_PHONG:
        return BASSET_MATERIAL_MODEL_PHONG;
    case BRESOURCE_MATERIAL_MODEL_COUNT:
        BWARN("bresource_material_model_to_basset converting count - did you mean to do this?");
        return BASSET_MATERIAL_MODEL_COUNT;
    case BRESOURCE_MATERIAL_MODEL_CUSTOM:
        return BASSET_MATERIAL_MODEL_CUSTOM;
    }
}

bresource_material_texture_map_channel basset_material_tex_map_channel_to_bresource(basset_material_texture_map_channel channel)
{
    switch (channel)
    {
    case BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_R:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_R;
    case BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_G:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_G;
    case BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_B:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_B;
    case BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_A:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_A;
    }
}

basset_material_texture_map_channel bresource_material_tex_map_channel_to_basset(bresource_material_texture_map_channel channel)
{
    switch (channel)
    {
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_R:
        return BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_R;
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_G:
        return BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_G;
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_B:
        return BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_B;
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_A:
        return BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_A;
    }
}

bresource_material_texture basset_material_texture_to_bresource(basset_material_texture texture)
{
    bresource_material_texture t;
    t.resource_name = texture.resource_name;
    t.sampler_name = texture.sampler_name;
    t.package_name = texture.package_name;
    t.channel = basset_material_tex_map_channel_to_bresource(texture.channel);
    return t;
}

basset_material_texture bresource_material_texture_to_basset(bresource_material_texture texture)
{
    basset_material_texture t;
    t.resource_name = texture.resource_name;
    t.sampler_name = texture.sampler_name;
    t.package_name = texture.package_name;
    t.channel = bresource_material_tex_map_channel_to_basset(texture.channel);
    return t;
}

bresource_material_sampler basset_material_sampler_to_bresource(basset_material_sampler sampler)
{
    bresource_material_sampler s;
    s.name = sampler.name;
    s.filter_mag = sampler.filter_mag;
    s.filter_min = sampler.filter_min;
    s.repeat_u = sampler.repeat_u;
    s.repeat_v = sampler.repeat_v;
    s.repeat_w = sampler.repeat_w;
    return s;
}

basset_material_sampler bresource_material_sampler_to_basset(bresource_material_sampler sampler)
{
    basset_material_sampler s;
    s.name = sampler.name;
    s.filter_mag = sampler.filter_mag;
    s.filter_min = sampler.filter_min;
    s.repeat_u = sampler.repeat_u;
    s.repeat_v = sampler.repeat_v;
    s.repeat_w = sampler.repeat_w;
    return s;
}
