#include "asset_handler_audio.h"

#include <assets/basset_importer_registry.h>
#include <assets/basset_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <serializers/basset_binary_audio_serializer.h>
#include <strings/bstring.h>

void asset_handler_audio_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = true;
    self->request_asset = 0;
    self->release_asset = asset_handler_audio_release_asset;
    self->type = BASSET_TYPE_AUDIO;
    self->type_name = BASSET_TYPE_NAME_AUDIO;
    self->binary_serialize = basset_binary_audio_serialize;
    self->binary_deserialize = basset_binary_audio_deserialize;
    self->text_serialize = 0;
    self->text_deserialize = 0;
}

void asset_handler_audio_release_asset(struct asset_handler* self, struct basset* asset)
{
    if (asset)
    {
        basset_audio* typed_asset = (basset_audio*)asset;
        
        if (typed_asset->pcm_data_size && typed_asset->pcm_data)
        {
            bfree(typed_asset->pcm_data, typed_asset->pcm_data_size, MEMORY_TAG_ASSET);
            typed_asset->pcm_data = 0;
            typed_asset->pcm_data_size = 0;
        }
        // Asset type-specific data cleanup
        typed_asset->total_sample_count = 0;
        typed_asset->sample_rate = 0;
        typed_asset->channels = 0;
    }
}
