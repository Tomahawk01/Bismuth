#include "asset_handler_heightmap_terrain.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <platform/vfs.h>
#include <serializers/basset_heightmap_terrain_serializer.h>
#include <strings/bstring.h>

#include "systems/asset_system.h"
#include "systems/material_system.h"

void asset_handler_heightmap_terrain_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->request_asset = asset_handler_heightmap_terrain_request_asset;
    self->release_asset = asset_handler_heightmap_terrain_release_asset;
    self->type = BASSET_TYPE_HEIGHTMAP_TERRAIN;
    self->type_name = BASSET_TYPE_NAME_HEIGHTMAP_TERRAIN;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = basset_heightmap_terrain_serialize;
    self->text_deserialize = basset_heightmap_terrain_deserialize;
}

void asset_handler_heightmap_terrain_request_asset(struct asset_handler* self, struct basset* asset, void* listener_instance, PFN_basset_on_result user_callback)
{
    struct vfs_state* vfs_state = engine_systems_get()->vfs_system_state;
    // Create and pass along a context
    // NOTE: The VFS takes a copy of this context, so the lifecycle doesn't matter
    asset_handler_request_context context = {0};
    context.asset = asset;
    context.handler = self;
    context.listener_instance = listener_instance;
    context.user_callback = user_callback;
    vfs_request_asset(vfs_state, &asset->meta, false, false, sizeof(asset_handler_request_context), &context, asset_handler_base_on_asset_loaded);
}

void asset_handler_heightmap_terrain_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_heightmap_terrain* typed_asset = (basset_heightmap_terrain*)asset;
    if (typed_asset->heightmap_filename)
    {
        string_free(typed_asset->heightmap_filename);
        typed_asset->heightmap_filename = 0;
    }
    if (typed_asset->material_count && typed_asset->material_names)
    {
        for (u32 i = 0; i < typed_asset->material_count; ++i)
        {
            const char* material_name = typed_asset->material_names[i];
            if (material_name)
            {
                string_free(material_name);
                material_name = 0;
            }
        }
        bfree(typed_asset->material_names, sizeof(const char*) * typed_asset->material_count, MEMORY_TAG_ARRAY);
        typed_asset->material_names = 0;
        typed_asset->material_count = 0;
    }
}
