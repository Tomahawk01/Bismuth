#include "asset_handler_image.h"

#include "systems/asset_system.h"

#include <assets/basset_importer_registry.h>
#include <assets/basset_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <serializers/basset_binary_image_serializer.h>
#include <strings/bstring.h>

void asset_handler_image_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = true;
    self->request_asset = 0;
    self->release_asset = asset_handler_image_release_asset;
    self->type = BASSET_TYPE_IMAGE;
    self->type_name = BASSET_TYPE_NAME_IMAGE;
    self->binary_serialize = basset_binary_image_serialize;
    self->binary_deserialize = basset_binary_image_deserialize;
    self->text_serialize = 0;
    self->text_deserialize = 0;
    self->size = sizeof(basset_image);
}

void asset_handler_image_release_asset(struct asset_handler* self, struct basset* asset)
{
    if (asset)
    {
        basset_image* typed_asset = (basset_image*)asset;
        // Asset type-specific data cleanup
        typed_asset->format = BASSET_IMAGE_FORMAT_UNDEFINED;
        typed_asset->width = 0;
        typed_asset->height = 0;
        typed_asset->mip_levels = 0;
        typed_asset->channel_count = 0;
    }
}
