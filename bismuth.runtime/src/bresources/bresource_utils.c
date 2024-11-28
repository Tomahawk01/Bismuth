#include "bresource_utils.h"
#include "assets/basset_types.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "resources/resource_types.h"

bresource_texture_format image_format_to_texture_format(basset_image_format format)
{
    switch (format)
    {
    case BASSET_IMAGE_FORMAT_RGBA8:
        return BRESOURCE_TEXTURE_FORMAT_RGBA8;
    case BASSET_IMAGE_FORMAT_UNDEFINED:
    default:
        return BRESOURCE_TEXTURE_FORMAT_UNKNOWN;
    }
}

basset_image_format texture_format_to_image_format(bresource_texture_format format)
{
    switch (format)
    {
    case BRESOURCE_TEXTURE_FORMAT_RGBA8:
        return BASSET_IMAGE_FORMAT_RGBA8;
    case BRESOURCE_TEXTURE_FORMAT_UNKNOWN:
    default:
        return BASSET_IMAGE_FORMAT_UNDEFINED;
    }
}

u8 channel_count_from_texture_format(bresource_texture_format format)
{
    switch (format)
    {
    case BRESOURCE_TEXTURE_FORMAT_RGBA8:
        return 4;
    case BRESOURCE_TEXTURE_FORMAT_RGB8:
        return 3;
    default:
        return 4;
    }
}

texture_channel bresource_texture_map_channel_to_texture_channel(bresource_material_texture_map_channel channel)
{
    switch (channel)
    {
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_R:
        return TEXTURE_CHANNEL_R;
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_G:
        return TEXTURE_CHANNEL_G;
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_B:
        return TEXTURE_CHANNEL_B;
    case BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_A:
        return TEXTURE_CHANNEL_A;
    }
}

bresource_material_texture_map_channel texture_channel_to_bresource_texture_map_channel(texture_channel channel)
{
    switch (channel)
    {
    case TEXTURE_CHANNEL_R:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_R;
    case TEXTURE_CHANNEL_G:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_G;
    case TEXTURE_CHANNEL_B:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_B;
    case TEXTURE_CHANNEL_A:
        return BRESOURCE_MATERIAL_TEXTURE_MAP_CHANNEL_A;
    }
}

material_type bresource_material_type_to_material_type(bresource_material_type type)
{
    switch (type)
    {
    case BRESOURCE_MATERIAL_TYPE_UNKNOWN:
        return MATERIAL_TYPE_UNKNOWN;
    case BRESOURCE_MATERIAL_TYPE_STANDARD:
        return MATERIAL_TYPE_STANDARD;
    case BRESOURCE_MATERIAL_TYPE_WATER:
        return MATERIAL_TYPE_WATER;
    case BRESOURCE_MATERIAL_TYPE_BLENDED:
        return MATERIAL_TYPE_BLENDED;
    case BRESOURCE_MATERIAL_TYPE_CUSTOM:
        return MATERIAL_TYPE_CUSTOM;
    case BRESOURCE_MATERIAL_TYPE_COUNT:
        BERROR("bresource_material_type_to_material_type - Don't try to convert 'count'");
        return MATERIAL_TYPE_UNKNOWN;
    }
}

bresource_material_type material_type_to_bresource_material_type(bresource_material_type type)
{
    switch (type)
    {
    case MATERIAL_TYPE_UNKNOWN:
        return BRESOURCE_MATERIAL_TYPE_UNKNOWN;
    case MATERIAL_TYPE_STANDARD:
        return BRESOURCE_MATERIAL_TYPE_STANDARD;
    case MATERIAL_TYPE_WATER:
        return BRESOURCE_MATERIAL_TYPE_WATER;
    case MATERIAL_TYPE_BLENDED:
        return BRESOURCE_MATERIAL_TYPE_BLENDED;
    case MATERIAL_TYPE_CUSTOM:
        return BRESOURCE_MATERIAL_TYPE_CUSTOM;
    case MATERIAL_TYPE_COUNT:
        BERROR("material_type_to_bresource_material_type - Don't try to convert 'count'");
        return BRESOURCE_MATERIAL_TYPE_UNKNOWN;
    }
}

material_model bresource_material_model_to_material_model(bresource_material_model model)
{
    switch (model)
    {
    case BRESOURCE_MATERIAL_MODEL_UNLIT:
        return MATERIAL_MODEL_UNLIT;
    case BRESOURCE_MATERIAL_MODEL_PBR:
        return MATERIAL_MODEL_PBR;
    case BRESOURCE_MATERIAL_MODEL_PHONG:
        return MATERIAL_MODEL_PHONG;
    case BRESOURCE_MATERIAL_MODEL_CUSTOM:
        return MATERIAL_MODEL_CUSTOM;
    case BRESOURCE_MATERIAL_MODEL_COUNT:
        BERROR("bresource_material_model_to_material_model - Don't try to convert 'count'");
        return MATERIAL_MODEL_UNLIT;
    }
}

bresource_material_model material_model_to_bresource_material_model(material_model model)
{
    switch (model)
    {
    case MATERIAL_MODEL_UNLIT:
        return BRESOURCE_MATERIAL_MODEL_UNLIT;
    case MATERIAL_MODEL_PBR:
        return BRESOURCE_MATERIAL_MODEL_PBR;
    case MATERIAL_MODEL_PHONG:
        return BRESOURCE_MATERIAL_MODEL_PHONG;
    case MATERIAL_MODEL_CUSTOM:
        return BRESOURCE_MATERIAL_MODEL_CUSTOM;
    case MATERIAL_MODEL_COUNT:
        return BRESOURCE_MATERIAL_MODEL_UNLIT;
        BERROR("material_model_to_bresource_material_model - Don't try to convert 'count'");
    }
}
