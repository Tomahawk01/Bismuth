#include "asset_handler_binary.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <strings/bstring.h>

#include "systems/asset_system.h"
#include "systems/material_system.h"

// fake a binary "serializer", which really just takes a copy of the data
static void* basset_binary_serialize(const basset* asset, u64* out_size)
{
    if (asset->type != BASSET_TYPE_BINARY)
    {
        BERROR("basset_binary_serialize requires a binary asset to serialize");
        return 0;
    }

    basset_binary* typed_asset = (basset_binary*)asset;

    void* out_data = ballocate(typed_asset->size, MEMORY_TAG_ASSET);
    *out_size = typed_asset->size;
    bcopy_memory(out_data, typed_asset->content, typed_asset->size);
    return out_data;
}

static b8 basset_binary_deserialize(u64 size, const void* data, basset* out_asset)
{
    if (!data || !out_asset || !size)
    {
        BERROR("basset_binary_deserialize requires valid pointers to data and out_asset as well as a nonzero size");
        return false;
    }

    if (out_asset->type != BASSET_TYPE_BINARY)
    {
        BERROR("basset_binary_serialize requires a binary asset to serialize");
        return 0;
    }

    basset_binary* typed_asset = (basset_binary*)out_asset;
    typed_asset->size = size;
    void* content = ballocate(typed_asset->size, MEMORY_TAG_ASSET);
    bcopy_memory(content, data, typed_asset->size);
    typed_asset->content = content;

    return true;
}

void asset_handler_binary_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = true;
    self->request_asset = 0;
    self->release_asset = asset_handler_binary_release_asset;
    self->type = BASSET_TYPE_BINARY;
    self->type_name = BASSET_TYPE_NAME_BINARY;
    self->binary_serialize = basset_binary_serialize;
    self->binary_deserialize = basset_binary_deserialize;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->size = sizeof(basset_binary);
}

void asset_handler_binary_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_binary* typed_asset = (basset_binary*)asset;
    if (typed_asset->content)
    {
        string_free(typed_asset->content);
        typed_asset->content = 0;
    }
}
