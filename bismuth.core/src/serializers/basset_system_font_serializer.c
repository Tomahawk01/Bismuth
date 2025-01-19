#include "basset_system_font_serializer.h"

#include "assets/basset_types.h"

#include "logger.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "strings/bname.h"

#define SYSTEM_FONT_FORMAT_VERSION 1

const char* basset_system_font_serialize(const basset* asset)
{
    if (!asset)
    {
        BERROR("basset_system_font_serialize requires an asset to serialize!");
        return 0;
    }

    basset_system_font* typed_asset = (basset_system_font*)asset;
    const char* out_str = 0;

    // Setup the BSON tree to serialize below
    bson_tree tree = {0};
    tree.root = bson_object_create();

    // version
    if (!bson_object_value_add_int(&tree.root, "version", SYSTEM_FONT_FORMAT_VERSION))
    {
        BERROR("Failed to add version, which is a required field");
        goto cleanup_bson;
    }

    // ttf_asset_name
    if (!bson_object_value_add_string(&tree.root, "ttf_asset_name", bname_string_get(typed_asset->ttf_asset_name)))
    {
        BERROR("Failed to add ttf_asset_name, which is a required field");
        goto cleanup_bson;
    }

    // ttf_asset_package_name
    if (!bson_object_value_add_bname_as_string(&tree.root, "ttf_asset_package_name", typed_asset->ttf_asset_package_name))
    {
        BERROR("Failed to add ttf_asset_package_name, which is a required field");
        goto cleanup_bson;
    }

    // faces
    bson_array faces_array = bson_array_create();
    for (u32 i = 0; i < typed_asset->face_count; ++i)
    {
        if (!bson_array_value_add_bname_as_string(&faces_array, typed_asset->faces[i].name))
        {
            BWARN("Unable to set face name at index %u. Skipping...", i);
            continue;
        }
    }
    if (!bson_object_value_add_array(&tree.root, "faces", faces_array))
    {
        BERROR("Failed to add faces, which is a required field");
        goto cleanup_bson;
    }

    out_str = bson_tree_to_string(&tree);
    if (!out_str)
        BERROR("Failed to serialize system_font to string. See logs for details");

cleanup_bson:
    bson_tree_cleanup(&tree);

    return out_str;
}

b8 basset_system_font_deserialize(const char* file_text, basset* out_asset)
{
    if (out_asset)
    {
        b8 success = false;
        basset_system_font* typed_asset = (basset_system_font*)out_asset;

        // Deserialize the loaded asset data
        bson_tree tree = {0};
        if (!bson_tree_from_string(file_text, &tree))
        {
            BERROR("Failed to parse asset data for system_font. See logs for details");
            goto cleanup_bson;
        }

        // version
        i64 version = 0;
        if (!bson_object_property_value_get_int(&tree.root, "version", &version))
        {
            BERROR("Failed to parse version, which is a required field");
            goto cleanup_bson;
        }
        typed_asset->base.meta.version = (u32)version;

        // ttf_asset_name
        if (!bson_object_property_value_get_string_as_bname(&tree.root, "ttf_asset_name", &typed_asset->ttf_asset_name))
        {
            BERROR("Failed to parse ttf_asset_name, which is a required field");
            goto cleanup_bson;
        }

        // ttf_asset_package_name
        if (!bson_object_property_value_get_string_as_bname(&tree.root, "ttf_asset_package_name", &typed_asset->ttf_asset_package_name))
        {
            BERROR("Failed to get ttf_asset_package_name, which is a required field");
            goto cleanup_bson;
        }

        // Faces array
        bson_array face_array = {0};
        if (!bson_object_property_value_get_array(&tree.root, "faces", &face_array))
        {
            BERROR("Failed to parse faces, which is a required field");
            goto cleanup_bson;
        }

        // Get the number of elements
        if (!bson_array_element_count_get(&face_array, (u32*)(&typed_asset->face_count)))
        {
            BERROR("Failed to parse face count. Invalid format?");
            goto cleanup_bson;
        }

        // Setup the new array
        typed_asset->faces = ballocate(sizeof(basset_system_font_face) * typed_asset->face_count, MEMORY_TAG_ARRAY);
        for (u32 i = 0; i < typed_asset->face_count; ++i)
        {
            if (!bson_array_element_value_get_string_as_bname(&face_array, i, &typed_asset->faces[i].name))
            {
                BWARN("Unable to read face name at index %u. Skipping...", i);
                continue;
            }
        }

        success = true;
    cleanup_bson:
        bson_tree_cleanup(&tree);
        if (!success)
        {
            if (typed_asset->face_count && typed_asset->faces)
            {
                bfree(typed_asset->faces, sizeof(basset_system_font_face) * typed_asset->face_count, MEMORY_TAG_ARRAY);
                typed_asset->faces = 0;
                typed_asset->face_count = 0;
            }
        }
        return success;
    }

    BERROR("basset_system_font_deserialize serializer requires an asset to deserialize to!");
    return false;
}
