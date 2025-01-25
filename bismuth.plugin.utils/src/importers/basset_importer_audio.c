#include "basset_importer_audio.h"

#include <assets/basset_types.h>
#include <core/engine.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <serializers/basset_binary_audio_serializer.h>
#include <stdlib.h>

// Loading vorbis files
#include "strings/bname.h"
#include "strings/bstring.h"
#include "vendor/stb_vorbis.h"
// Loading mp3 files
#define MINIMP3_IMPLEMENTATION
// #define MINIMP3_NO_STDIO
#include "vendor/minimp3_ex.h"

b8 basset_importer_audio_import(const struct basset_importer* self, u64 data_size, const void* data, void* params, struct basset* out_asset)
{
    if (!self || !data_size || !data)
    {
        BERROR("basset_importer_audio_import requires valid pointers to self and data, as well as a nonzero data_size");
        return false;
    }

    basset_audio* typed_asset = (basset_audio*)out_asset;

    if (strings_equali(self->source_type, "mp3"))
    {
        BTRACE("Importing MP3 asset '%s'...", bname_string_get(out_asset->name));
        typed_asset->pcm_data_size = 0;
        typed_asset->pcm_data = 0;
        typed_asset->total_sample_count = 0;

        // Initialize the decoder
        mp3dec_t mp3_decoder;
        mp3dec_init(&mp3_decoder);

        // HACK: test
        // mp3dec_file_info_t file_info;
        // const char* full_file_path = "../testbed.bapp/assets/music/source/bg_song1.mp3";
        // i32 err = mp3dec_load(&mp3_decoder, full_file_path, &file_info, 0, 0);
        // if (err < 0)
        // {
        //     BERROR("Error decoding MP3");
        //     return false;
        // }
        // typed_asset->total_sample_count = file_info.samples;
        // typed_asset->channels = file_info.channels;
        // typed_asset->sample_rate = file_info.hz;
        // typed_asset->pcm_data_size = file_info.samples * sizeof(mp3d_sample_t);
        // typed_asset->pcm_data = ballocate(typed_asset->pcm_data_size, MEMORY_TAG_ASSET);
        // BDEBUG("Decoded mp3 - channels: %d, samples: %llu, sample_rate/freq: %dHz, avg kbit/s rate: %d, size: %llu", file_info.channels, file_info.samples, file_info.hz, file_info.avg_bitrate_kbps, typed_asset->pcm_data_size);
        // bcopy_memory(typed_asset->pcm_data, file_info.buffer, typed_asset->pcm_data_size);
        // if(typed_asset->pcm_data) {
        // }

        // TODO: new
        mp3dec_file_info_t file_info;
        i32 err = mp3dec_load_buf(&mp3_decoder, (u8*)data, data_size, &file_info, 0, 0);
        if (err < 0)
        {
            BERROR("Error decoding MP3");
            return false;
        }

        BINFO("Decoded %llu samples successfully", file_info.samples);

        typed_asset->total_sample_count = file_info.samples;
        typed_asset->channels = file_info.channels;
        typed_asset->sample_rate = file_info.hz;
        typed_asset->pcm_data_size = file_info.samples * sizeof(mp3d_sample_t);
        typed_asset->pcm_data = ballocate(typed_asset->pcm_data_size, MEMORY_TAG_ASSET);
        BDEBUG("Decoded mp3 - channels: %d, samples: %llu, sample_rate/freq: %dHz, avg kbit/s rate: %d, size: %llu", file_info.channels, file_info.samples, file_info.hz, file_info.avg_bitrate_kbps, typed_asset->pcm_data_size);
        bcopy_memory(typed_asset->pcm_data, file_info.buffer, typed_asset->pcm_data_size);

        // TODO: old
        // mp3dec_frame_info_t frame_info;
        // while (typed_asset->pcm_data_size < data_size)
        // {
        //     i16 pcm[MINIMP3_MAX_SAMPLES_PER_FRAME] = {0};
        //     i32 sample_count = mp3dec_decode_frame(&mp3_decoder, ((u8*)data) + typed_asset->pcm_data_size, data_size - typed_asset->pcm_data_size, pcm, &frame_info);
        //     if (sample_count < 0)
        //     {
        //         BERROR("Error decoding MP3 frame");
        //         break;
        //     }
        //     typed_asset->total_sample_count += sample_count;
        
        //     typed_asset->pcm_data = (i16*)breallocate(typed_asset->pcm_data, typed_asset->pcm_data_size, typed_asset->pcm_data_size + frame_info.frame_bytes, MEMORY_TAG_ASSET);
        //     if (!typed_asset->pcm_data)
        //     {
        //         BERROR("Failed to reallocate more memory for PCM data!");
        //         break;
        //     }

        //     // Append the data
        //     bcopy_memory(((u8*)typed_asset->pcm_data) + typed_asset->pcm_data_size, pcm, frame_info.frame_bytes);
        //     typed_asset->pcm_data_size += frame_info.frame_bytes;
        // }

        // // Fill out the baf audio data
        // typed_asset->channels = frame_info.channels;
        // typed_asset->sample_rate = frame_info.hz;
    }
    else if (strings_equali(self->source_type, "ogg"))
    {
        BTRACE("Importing OGG Vorbis asset '%s'...", bname_string_get(out_asset->name));

        i16* decoded_pcm_data = 0;
        i32 total_samples = stb_vorbis_decode_memory(data, data_size, &typed_asset->channels, (i32*)&typed_asset->sample_rate, &decoded_pcm_data);
        if (!decoded_pcm_data || total_samples < 0)
        {
            BERROR("Failed to import OGG Vorbis file");
            return false;
        }
        // Make sure this is a multiple of 4. If not, loading into the buffer can fail
        total_samples += (total_samples % 4);

        typed_asset->total_sample_count = total_samples;
        typed_asset->pcm_data_size = typed_asset->total_sample_count * sizeof(i16);
        typed_asset->pcm_data = ballocate(typed_asset->pcm_data_size, MEMORY_TAG_ASSET);
        bcopy_memory(typed_asset->pcm_data, decoded_pcm_data, typed_asset->pcm_data_size);
        free(decoded_pcm_data);
    }
    else if (strings_equali(self->source_type, "wav"))
    {
        BTRACE("Importing WAV Vorbis asset '%s'...", bname_string_get(out_asset->name));
        // FIXME: support wav
        BFATAL("wav not yet supported");
    }
    else
    {
        BFATAL("Unsupported audio source file format '%s'", self->source_type);
    }

    // Serialize and write to the VFS
    struct vfs_state* vfs = engine_systems_get()->vfs_system_state;

    u64 serialized_block_size = 0;
    void* serialized_block = basset_binary_audio_serialize(out_asset, &serialized_block_size);
    if (!serialized_block)
    {
        BERROR("Binary audio serialization failed, check logs");
        return false;
    }
    
    b8 success = true;
    if (!vfs_asset_write(vfs, out_asset, true, serialized_block_size, serialized_block))
    {
        BERROR("Failed to write Binary Audio asset data to VFS. See logs for details");
        success = false;
    }

    if (serialized_block)
        bfree(serialized_block, serialized_block_size, MEMORY_TAG_SERIALIZER);

    return success;
}
