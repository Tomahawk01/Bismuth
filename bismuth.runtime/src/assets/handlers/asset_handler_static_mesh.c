#include "asset_handler_static_mesh.h"

#include "math/bmath.h"
#include "memory/bmemory.h"
#include "strings/bstring.h"
#include "systems/asset_system.h"

#include <assets/basset_importer_registry.h>
#include <assets/basset_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <platform/vfs.h>
#include <serializers/basset_binary_static_mesh_serializer.h>

void asset_handler_static_mesh_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = true;
    self->request_asset = 0;
    self->release_asset = asset_handler_static_mesh_release_asset;
    self->type = BASSET_TYPE_STATIC_MESH;
    self->type_name = BASSET_TYPE_NAME_STATIC_MESH;
    self->binary_serialize = basset_binary_static_mesh_serialize;
    self->binary_deserialize = basset_binary_static_mesh_deserialize;
    self->text_serialize = 0;
    self->text_deserialize = 0;
}

void asset_handler_static_mesh_release_asset(struct asset_handler* self, struct basset* asset)
{
    if (asset)
    {
        basset_static_mesh* typed_asset = (basset_static_mesh*)asset;
        // Asset type-specific data cleanup
        if (typed_asset->geometries && typed_asset->geometry_count)
        {
            for (u32 i = 0; i < typed_asset->geometry_count; ++i)
            {
                basset_static_mesh_geometry* g = &typed_asset->geometries[i];
                if (g->vertices && g->vertex_count)
                    bfree(g->vertices, sizeof(g->vertices[0]) * g->vertex_count, MEMORY_TAG_ARRAY);
                if (g->indices && g->index_count)
                    bfree(g->indices, sizeof(g->indices[0]) * g->index_count, MEMORY_TAG_ARRAY);
            }
            bfree(typed_asset->geometries, sizeof(typed_asset->geometries[0]) * typed_asset->geometry_count, MEMORY_TAG_ARRAY);
            typed_asset->geometries = 0;
            typed_asset->geometry_count = 0;
        }
        typed_asset->center = vec3_zero();
        typed_asset->extents.min = typed_asset->extents.max = vec3_zero();
    }
}
