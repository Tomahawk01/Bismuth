#include "basset_binary_image_serializer.h"

#include "assets/basset_types.h"
#include "assets/basset_utils.h"
#include "logger.h"
#include "memory/bmemory.h"

typedef struct binary_image_header
{
    // The base binary asset header. Must always be the first member
    binary_asset_header base;
    // The image format. cast to basset_image_format
    u32 format;
    // The image width in pixels
    u32 width;
    // The image height in pixels
    u32 height;
    // The number of mip levels for the asset
    u8 mip_levels;
    // Padding used to keep the structure size 32-bit aligned
    u8 padding[3];
} binary_image_header;

BAPI void* basset_binary_image_serialize(const basset* asset, u64* out_size)
{
    if (!asset)
    {
        BERROR("Cannot serialize without an asset!");
        return 0;
    }

    if (asset->type != BASSET_TYPE_IMAGE)
    {
        BERROR("Cannot serialize a non-image asset using the image serializer");
        return 0;
    }

    basset_image* typed_asset = (basset_image*)asset;

    binary_image_header header = {0};
    // Base attributes
    header.base.magic = ASSET_MAGIC;
    header.base.type = (u32)asset->type;
    header.base.data_block_size = typed_asset->pixel_array_size;
    // Always write the most current version
    header.base.version = 1;

    header.height = typed_asset->height;
    header.width = typed_asset->width;
    header.mip_levels = typed_asset->mip_levels;
    header.format = (u32)typed_asset->format;

    *out_size = sizeof(binary_image_header) + typed_asset->pixel_array_size;

    void* block = ballocate(*out_size, MEMORY_TAG_SERIALIZER);
    bcopy_memory(block, &header, sizeof(binary_image_header));
    bcopy_memory(((u8*)block) + sizeof(binary_image_header), typed_asset->pixels, typed_asset->pixel_array_size);

    return block;
}

BAPI b8 basset_binary_image_deserialize(u64 size, const void* block, basset* out_asset)
{
    if (!size || !block || !out_asset)
    {
        BERROR("Cannot deserialize without a nonzero size, block of memory and an asset to write to");
        return false;
    }

    const binary_image_header* header = block;
    if (header->base.magic != ASSET_MAGIC)
    {
        BERROR("Memory is not a Bismuth binary asset");
        return false;
    }

    basset_type type = (basset_type)header->base.type;
    if (type != BASSET_TYPE_IMAGE)
    {
        BERROR("Memory is not a Bismuth image asset");
        return false;
    }

    u64 expected_size = header->base.data_block_size + sizeof(binary_image_header);
    if (expected_size != size)
    {
        BERROR("Deserialization failure: Expected block size/block size mismatch: %llu/%llu", expected_size, size);
        return false;
    }

    basset_image* out_image = (basset_image*)out_asset;

    out_image->height = header->height;
    out_image->width = header->width;
    out_image->mip_levels = header->mip_levels;
    out_image->format = header->format;
    out_image->pixel_array_size = header->base.data_block_size;
    out_image->base.meta.version = header->base.version;
    out_image->base.type = type;
    out_image->channel_count = channel_count_from_image_format(header->format);

    // Copy the actual image data block
    out_image->pixels = ballocate(out_image->pixel_array_size, MEMORY_TAG_ASSET);
    bcopy_memory(out_image->pixels, ((u8*)block) + sizeof(binary_image_header), header->base.data_block_size);

    return true;
}
