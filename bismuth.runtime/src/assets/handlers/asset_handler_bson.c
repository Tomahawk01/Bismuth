#include "asset_handler_bson.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <serializers/basset_bson_serializer.h>
#include <strings/bstring.h>

#include "systems/asset_system.h"
#include "systems/material_system.h"

void asset_handler_bson_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->request_asset = asset_handler_bson_request_asset;
    self->release_asset = asset_handler_bson_release_asset;
    self->type = BASSET_TYPE_BSON;
    self->type_name = BASSET_TYPE_NAME_BSON;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = basset_bson_serialize;
    self->text_deserialize = basset_bson_deserialize;
}

void asset_handler_bson_request_asset(struct asset_handler* self, struct basset* asset, void* listener_instance, PFN_basset_on_result user_callback)
{
    struct vfs_state* vfs_state = engine_systems_get()->vfs_system_state;
    // Create and pass along a conbson
    // NOTE: The VFS takes a copy of this conbson, so the lifecycle doesn't matter
    asset_handler_request_context context = {0};
    context.asset = asset;
    context.handler = self;
    context.listener_instance = listener_instance;
    context.user_callback = user_callback;
    vfs_request_asset(vfs_state, &asset->meta, false, false, sizeof(asset_handler_request_context), &context, asset_handler_base_on_asset_loaded);
}

void asset_handler_bson_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_bson* typed_asset = (basset_bson*)asset;
    if (typed_asset->source_text)
    {
        string_free(typed_asset->source_text);
        typed_asset->source_text = 0;
    }
    bson_tree_cleanup(&typed_asset->tree);
}
