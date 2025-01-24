#include "basset_binary_audio_serializer.h"

#include "assets/basset_types.h"
#include "logger.h"
#include "memory/bmemory.h"

typedef struct binary_audio_header
{
    // The base binary asset header. Must always be the first member
    binary_asset_header base;
    // The number of channels (i.e. 1 for mono or 2 for stereo)
    i32 channels;
    // The sample rate of the audio/music (i.e. 44100)
    u32 sample_rate;
    u32 total_sample_count;
    u64 pcm_data_size;
} binary_audio_header;

BAPI void* basset_binary_audio_serialize(const basset* asset, u64* out_size)
{
    if (!asset)
    {
        BERROR("Cannot serialize without an asset");
        return 0;
    }

    if (asset->type != BASSET_TYPE_AUDIO)
    {
        BERROR("Cannot serialize a non-audio asset using the audio serializer");
        return 0;
    }

    basset_audio* typed_asset = (basset_audio*)asset;

    binary_audio_header header = {0};
    // Base attributes
    header.base.magic = ASSET_MAGIC;
    header.base.type = (u32)asset->type;
    header.base.data_block_size = typed_asset->pcm_data_size;
    // Always write the most current version
    header.base.version = 1;

    header.pcm_data_size = typed_asset->pcm_data_size;
    header.sample_rate = typed_asset->sample_rate;
    header.channels = typed_asset->channels;
    header.total_sample_count = typed_asset->total_sample_count;

    *out_size = sizeof(binary_audio_header) + typed_asset->pcm_data_size;

    void* block = ballocate(*out_size, MEMORY_TAG_SERIALIZER);
    bcopy_memory(block, &header, sizeof(binary_audio_header));
    bcopy_memory(((u8*)block) + sizeof(binary_audio_header), typed_asset->pcm_data, typed_asset->pcm_data_size);

    return block;
}

BAPI b8 basset_binary_audio_deserialize(u64 size, const void* block, basset* out_asset)
{
    if (!size || !block || !out_asset)
    {
        BERROR("Cannot deserialize without a nonzero size, block of memory and an asset to write to");
        return false;
    }

    const binary_audio_header* header = block;
    if (header->base.magic != ASSET_MAGIC)
    {
        BERROR("Memory is not a Bismuth binary asset");
        return false;
    }

    basset_type type = (basset_type)header->base.type;
    if (type != BASSET_TYPE_AUDIO)
    {
        BERROR("Memory is not a Bismuth audio asset");
        return false;
    }

    u64 expected_size = header->base.data_block_size + sizeof(binary_audio_header);
    if (expected_size != size)
    {
        BERROR("Deserialization failure: Expected block size/block size mismatch: %llu/%llu", expected_size, size);
        return false;
    }

    basset_audio* out_audio = (basset_audio*)out_asset;

    out_audio->base.type = type;
    out_audio->base.meta.version = header->base.version;
    out_audio->channels = header->channels;
    out_audio->total_sample_count = header->total_sample_count;
    out_audio->sample_rate = header->sample_rate;
    out_audio->pcm_data_size = header->pcm_data_size;

    // Copy the actual audio data block
    out_audio->pcm_data = ballocate(out_audio->pcm_data_size, MEMORY_TAG_ASSET);
    bcopy_memory(out_audio->pcm_data, ((u8*)block) + sizeof(binary_audio_header), header->base.data_block_size);
    
    return true;
}
