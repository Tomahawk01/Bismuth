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
