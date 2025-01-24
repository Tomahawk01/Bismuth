#include "bresource_utils.h"

#include "assets/basset_types.h"
#include "bresources/bresource_types.h"
#include "logger.h"

texture_format image_format_to_texture_format(basset_image_format format)
{
    switch (format)
    {
    case BASSET_IMAGE_FORMAT_RGBA8:
        return TEXTURE_FORMAT_RGBA8;
    case BASSET_IMAGE_FORMAT_UNDEFINED:
    default:
        return TEXTURE_FORMAT_UNKNOWN;
    }
}

basset_image_format texture_format_to_image_format(texture_format format)
{
    switch (format)
    {
    case TEXTURE_FORMAT_RGBA8:
        return BASSET_IMAGE_FORMAT_RGBA8;
    case TEXTURE_FORMAT_UNKNOWN:
    default:
        return BASSET_IMAGE_FORMAT_UNDEFINED;
    }
}

u8 channel_count_from_texture_format(texture_format format)
{
    switch (format)
    {
    case TEXTURE_FORMAT_RGBA8:
        return 4;
    case TEXTURE_FORMAT_RGB8:
        return 3;
    default:
        return 4;
    }
}

const char* bresource_type_to_string(bresource_type type)
{
    switch (type)
    {
    case BRESOURCE_TYPE_UNKNOWN:
        return "unknown";
    case BRESOURCE_TYPE_TEXT:
        return "text";
    case BRESOURCE_TYPE_BINARY:
        return "binary";
    case BRESOURCE_TYPE_TEXTURE:
        return "texture";
    case BRESOURCE_TYPE_MATERIAL:
        return "material";
    case BRESOURCE_TYPE_SHADER:
        return "shader";
    case BRESOURCE_TYPE_STATIC_MESH:
        return "static_mesh";
    case BRESOURCE_TYPE_SKELETAL_MESH:
        return "skeletal_mesh";
    case BRESOURCE_TYPE_BITMAP_FONT:
        return "bitmap_font";
    case BRESOURCE_TYPE_SYSTEM_FONT:
        return "system_font";
    case BRESOURCE_TYPE_SCENE:
        return "scene";
    case BRESOURCE_TYPE_HEIGHTMAP_TERRAIN:
        return "heightmap_terrain";
    case BRESOURCE_TYPE_VOXEL_TERRAIN:
        return "voxel_terrain";
    case BRESOURCE_TYPE_SOUND_EFFECT:
        return "sound_effect";
    case BRESOURCE_TYPE_MUSIC:
        return "music";
    case BRESOURCE_TYPE_COUNT:
    case BRESOURCE_KNOWN_TYPE_MAX:
        BERROR("Attempted to get string representation of count/max. Returning unknown");
        return "unknown";
    }
}
