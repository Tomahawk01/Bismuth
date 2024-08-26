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
    self->request_asset = asset_handler_image_request_asset;
    self->release_asset = asset_handler_image_release_asset;
    self->type = BASSET_TYPE_IMAGE;
    self->type_name = BASSET_TYPE_NAME_IMAGE;
    self->binary_serialize = basset_binary_image_serialize;
    self->binary_deserialize = basset_binary_image_deserialize;
    self->text_serialize = 0;
    self->text_deserialize = 0;
}

void asset_handler_image_request_asset(struct asset_handler* self, struct basset* asset, void* listener_instance, PFN_basset_on_result user_callback)
{
    struct vfs_state* vfs_state = engine_systems_get()->vfs_system_state;
    // Create and pass along a context
    // NOTE: The VFS takes a copy of this context, so the lifecycle doesn't matter
    asset_handler_request_context context = {0};
    context.asset = asset;
    context.handler = self;
    context.listener_instance = listener_instance;
    context.user_callback = user_callback;
    // Always request the primary asset first
    vfs_request_asset(vfs_state, &asset->meta, true, false, sizeof(asset_handler_request_context), &context, asset_handler_base_on_asset_loaded);
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
