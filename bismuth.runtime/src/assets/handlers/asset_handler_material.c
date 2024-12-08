#include "asset_handler_material.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <platform/vfs.h>
#include <serializers/basset_material_serializer.h>
#include <strings/bstring.h>

void asset_handler_material_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = false;
    self->request_asset = asset_handler_material_request_asset;
    self->release_asset = asset_handler_material_release_asset;
    self->type = BASSET_TYPE_MATERIAL;
    self->type_name = BASSET_TYPE_NAME_MATERIAL;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = basset_material_serialize;
    self->text_deserialize = basset_material_deserialize;
}

void asset_handler_material_request_asset(struct asset_handler* self, struct basset* asset, void* listener_instance, PFN_basset_on_result user_callback)
{
    struct vfs_state* vfs_state = engine_systems_get()->vfs_system_state;
    // Create and pass along a context
    // NOTE: The VFS takes a copy of this context, so the lifecycle doesn't matter
    asset_handler_request_context context = {0};
    context.asset = asset;
    context.handler = self;
    context.listener_instance = listener_instance;
    context.user_callback = user_callback;
    
    vfs_request_info request_info = {0};
    request_info.package_name = asset->package_name;
    request_info.asset_name = asset->name;
    request_info.is_binary = false;
    request_info.get_source = false;
    request_info.context_size = sizeof(asset_handler_request_context);
    request_info.context = &context;
    request_info.import_params = 0;
    request_info.import_params_size = 0;
    request_info.vfs_callback = asset_handler_base_on_asset_loaded;
    // TODO: Material resource hot reloading
    request_info.watch_for_hot_reload = false;

    vfs_request_asset(vfs_state, request_info);
}

void asset_handler_material_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_material* typed_asset = (basset_material*)asset;
    if (typed_asset->custom_sampler_count && typed_asset->custom_samplers)
        BFREE_TYPE_CARRAY(typed_asset->custom_samplers, bmaterial_sampler_config, typed_asset->custom_sampler_count);
}
