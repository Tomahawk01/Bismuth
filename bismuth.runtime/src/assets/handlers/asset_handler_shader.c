#include "asset_handler_shader.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <platform/vfs.h>
#include <serializers/basset_shader_serializer.h>
#include <strings/bstring.h>

#include "assets/basset_types.h"

void asset_handler_shader_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = false;
    self->request_asset = 0;
    self->release_asset = asset_handler_shader_release_asset;
    self->type = BASSET_TYPE_SHADER;
    self->type_name = BASSET_TYPE_NAME_SHADER;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = basset_shader_serialize;
    self->text_deserialize = basset_shader_deserialize;
}

void asset_handler_shader_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_shader* typed_asset = (basset_shader*)asset;
    // Stages
    if (typed_asset->stages && typed_asset->stage_count)
    {
        for (u32 i = 0; i < typed_asset->stage_count; ++i)
        {
            basset_shader_stage* stage = &typed_asset->stages[i];
            if (stage->source_asset_name)
                string_free(stage->source_asset_name);
            if (stage->package_name)
                string_free(stage->package_name);
        }
        bfree(typed_asset->stages, sizeof(basset_shader_stage*) * typed_asset->stage_count, MEMORY_TAG_ARRAY);
        typed_asset->stages = 0;
        typed_asset->stage_count = 0;
    }

    // Attributes
    if (typed_asset->attributes && typed_asset->attribute_count)
    {
        for (u32 i = 0; i < typed_asset->attribute_count; ++i)
        {
            basset_shader_attribute* attrib = &typed_asset->attributes[i];
            if (attrib->name)
                string_free(attrib->name);
        }
        bfree(typed_asset->attributes, sizeof(basset_shader_stage*) * typed_asset->attribute_count, MEMORY_TAG_ARRAY);
        typed_asset->attributes = 0;
        typed_asset->attribute_count = 0;
    }

    // Uniforms
    if (typed_asset->uniforms && typed_asset->uniform_count)
    {
        for (u32 i = 0; i < typed_asset->uniform_count; ++i)
        {
            basset_shader_uniform* attrib = &typed_asset->uniforms[i];
            if (attrib->name)
                string_free(attrib->name);
        }
        bfree(typed_asset->uniforms, sizeof(basset_shader_stage*) * typed_asset->uniform_count, MEMORY_TAG_ARRAY);
        typed_asset->uniforms = 0;
        typed_asset->uniform_count = 0;
    }
}
