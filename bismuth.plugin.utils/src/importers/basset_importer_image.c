#include "basset_importer_image.h"

#include <assets/basset_types.h>
#include <core/engine.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <serializers/basset_binary_image_serializer.h>

#define STB_IMAGE_IMPLEMENTATION
// Using our own filesystem
#define STBI_NO_STDIO
#include "vendor/stb_image.h"

b8 basset_importer_image_import(const struct basset_importer* self, u64 data_size, const void* data, void* params, struct basset* out_asset)
{
    if (!self || !data_size || !data)
    {
        BERROR("basset_importer_image_import requires valid pointers to self and data, as well as a nonzero data_size");
        return false;
    }

    // Defaults
    basset_image_import_options default_options = {0};
    default_options.flip_y = true;
    default_options.format = BASSET_IMAGE_FORMAT_RGBA8;

    basset_image_import_options* options = 0;
    if (!params)
    {
        // BERROR("basset_importer_image_import requires parameters to be present");
        // return false;
        BWARN("basset_importer_image_import - no params defined, using defaults");
        options = &default_options;
    }
    else
    {
        options = (basset_image_import_options*)params;
    }
    basset_image* typed_asset = (basset_image*)out_asset;

    // Determine channel count
    i32 required_channel_count = 0;
    u8 bits_per_channel = 0;
    switch (options->format)
    {
    case BASSET_IMAGE_FORMAT_RGBA8:
        required_channel_count = 4;
        bits_per_channel = 8;
        break;
    default:
        BWARN("Unrecognized image format requested - defaulting to 4 channels (RGBA)/8bpc");
        options->format = BASSET_IMAGE_FORMAT_RGBA8;
        required_channel_count = 4;
        bits_per_channel = 8;
        break;
    }

    // Set the "flip" as described in the options
    stbi_set_flip_vertically_on_load_thread(options->flip_y);

    // Load the image. NOTE: forcing 4 channels here for now
    i32 channel_count_rubbish = 0;
    u8* pixels = stbi_load_from_memory(data, data_size, (i32*)&typed_asset->width, (i32*)&typed_asset->height, &channel_count_rubbish, required_channel_count);
    typed_asset->channel_count = required_channel_count;
    if (!pixels)
    {
        BERROR("Image importer failed to import image '%s'", bstring_id_string_get(out_asset->meta.source_asset_path));
        return false;
    }

    u64 actual_size = (bits_per_channel / 8) * typed_asset->channel_count * typed_asset->width * typed_asset->height;
    typed_asset->pixel_array_size = actual_size;
    typed_asset->pixels = pixels;

    // NOTE: Querying is done below
    /* i32 result = stbi_info_from_memory(data, data_size, (i32*)&typed_asset->width, (i32*)&typed_asset->height, (i32*)&typed_asset->channel_count);
    if (result == 0)
    {
        BERROR("Failed to query image data from memory");
        goto image_loader_query_return;
    } */

    // The number of mip levels is calculated by first taking the largest dimension (either width or height),
    // figuring out how many times that number can be divided by 2,
    // taking the floor value (rounding down) and adding 1 to represent the base level.
    // This always leaves a value of at least 1
    typed_asset->mip_levels = (u32)(bfloor(blog2(BMAX(typed_asset->width, typed_asset->height))) + 1);

    // Serialize and write to the VFS
    struct vfs_state* vfs = engine_systems_get()->vfs_system_state;

    u64 serialized_block_size = 0;
    void* serialized_block = basset_binary_image_serialize(out_asset, &serialized_block_size);
    if (!serialized_block)
    {
        BERROR("Binary image serialization failed, check logs");
        return false;
    }

    b8 success = true;
    if (!vfs_asset_write(vfs, out_asset, true, serialized_block_size, serialized_block))
    {
        BERROR("Failed to write Binary Image asset data to VFS. See logs for details");
        success = false;
    }

    if (serialized_block)
        bfree(serialized_block, serialized_block_size, MEMORY_TAG_SERIALIZER);

    return success;
}
