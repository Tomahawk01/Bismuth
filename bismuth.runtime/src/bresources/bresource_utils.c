#include "bresource_utils.h"
#include "assets/basset_types.h"
#include "bresources/bresource_types.h"

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
