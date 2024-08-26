#include "basset_bson_serializer.h"

#include "assets/basset_types.h"
#include "containers/darray.h"
#include "core_render_types.h"
#include "logger.h"
#include "parsers/bson_parser.h"
#include "strings/bstring.h"
#include "utils/render_type_utils.h"

const char* basset_bson_serialize(const basset* asset)
{
    if (asset->type != BASSET_TYPE_BSON)
    {
        BERROR("basset_bson_serialize requires a bson asset to serialize");
        return 0;
    }

    basset_bson* typed_asset = (basset_bson*)asset;
    return bson_tree_to_string(&typed_asset->tree);
}

b8 basset_bson_deserialize(const char* file_text, basset* out_asset)
{
    if (!file_text || !out_asset)
    {
        BERROR("basset_bson_deserialize requires valid pointers to file_text and out_asset");
        return false;
    }

    if (out_asset->type != BASSET_TYPE_BSON)
    {
        BERROR("basset_bson_serialize requires a bson asset to serialize");
        return 0;
    }

    basset_bson* typed_asset = (basset_bson*)out_asset;
    if (!bson_tree_from_string(file_text, &typed_asset->tree))
    {
        BERROR("Failed to parse bson string. See logs for details");
        return false;
    }

    return true;
}
