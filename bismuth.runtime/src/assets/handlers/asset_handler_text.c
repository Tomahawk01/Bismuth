#include "asset_handler_text.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <platform/vfs.h>
#include <strings/bstring.h>

#include "systems/asset_system.h"
#include "systems/material_system.h"

static b8 basset_text_deserialize(const char* file_text, basset* out_asset);
static const char* basset_text_serialize(const basset* asset);

void asset_handler_text_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = false;
    self->request_asset = 0;
    self->release_asset = asset_handler_text_release_asset;
    self->type = BASSET_TYPE_TEXT;
    self->type_name = BASSET_TYPE_NAME_TEXT;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = basset_text_serialize;
    self->text_deserialize = basset_text_deserialize;
}

void asset_handler_text_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_text* typed_asset = (basset_text*)asset;
    if (typed_asset->content)
    {
        string_free(typed_asset->content);
        typed_asset->content = 0;
    }
}

static b8 basset_text_deserialize(const char* file_text, basset* out_asset)
{
    if (!file_text || !out_asset)
        return false;

    basset_text* typed_asset = (basset_text*)out_asset;
    typed_asset->content = string_duplicate(file_text);
    
    return true;
}

static const char* basset_text_serialize(const basset* asset)
{
    if (!asset)
        return 0;

    return string_duplicate(((basset_text*)asset)->content);
}
