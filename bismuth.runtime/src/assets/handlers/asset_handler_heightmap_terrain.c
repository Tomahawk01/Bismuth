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

void asset_handler_heightmap_terrain_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = false;
    self->request_asset = 0;
    self->release_asset = asset_handler_heightmap_terrain_release_asset;
    self->type = BASSET_TYPE_HEIGHTMAP_TERRAIN;
    self->type_name = BASSET_TYPE_NAME_HEIGHTMAP_TERRAIN;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = basset_heightmap_terrain_serialize;
    self->text_deserialize = basset_heightmap_terrain_deserialize;
    self->size = sizeof(basset_heightmap_terrain);
}

void asset_handler_heightmap_terrain_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_heightmap_terrain* typed_asset = (basset_heightmap_terrain*)asset;
    if (typed_asset->material_count && typed_asset->material_names)
    {
        bfree(typed_asset->material_names, sizeof(bname) * typed_asset->material_count, MEMORY_TAG_ARRAY);
        typed_asset->material_names = 0;
        typed_asset->material_count = 0;
    }
}
