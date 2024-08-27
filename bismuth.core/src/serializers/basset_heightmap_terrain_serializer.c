#include "basset_heightmap_terrain_serializer.h"

#include "assets/basset_types.h"

#include "logger.h"
#include "math/bmath.h"
#include "parsers/bson_parser.h"
#include "strings/bname.h"
#include "strings/bstring.h"

const char* basset_heightmap_terrain_serialize(const basset* asset)
{
    if (!asset)
    {
        BERROR("basset_heightmap_serialize requires an asset to serialize!");
        return 0;
    }

    basset_heightmap_terrain* typed_asset = (basset_heightmap_terrain*)asset;
    const char* out_str = 0;

    // Setup the BSON tree to serialize below
    bson_tree tree = {0};
    tree.root = bson_object_create();

    // version
    if (!bson_object_value_add_int(&tree.root, "version", typed_asset->base.meta.version))
    {
        BERROR("Failed to add version, which is a required field");
        goto cleanup_bson;
    }

    // heightmap_filename
    if (!bson_object_value_add_string(&tree.root, "heightmap_asset_name", bname_string_get(typed_asset->heightmap_asset_name)))
    {
        BERROR("Failed to add heightmap_asset_name, which is a required field");
        goto cleanup_bson;
    }

    // chunk_size
    if (!bson_object_value_add_int(&tree.root, "chunk_size", typed_asset->chunk_size))
    {
        BERROR("Failed to add chunk_size, which is a required field");
        goto cleanup_bson;
    }

    // tile_scale
    if (!bson_object_value_add_vec3(&tree.root, "tile_scale", typed_asset->tile_scale))
    {
        BERROR("Failed to add tile_scale, which is a required field");
        goto cleanup_bson;
    }

    // Material names array
    bson_array material_names_array = bson_array_create();
    for (u32 i = 0; i < typed_asset->material_count; ++i)
    {
        if (!bson_array_value_add_string(&material_names_array, bname_string_get(typed_asset->material_names[i])))
            BWARN("Unable to set material name at index %u, using default of '%s' instead", "default_terrain");
    }
    if (!bson_object_value_add_array(&tree.root, "material_names", material_names_array))
    {
        BERROR("Failed to add material_names, which is a required field");
        goto cleanup_bson;
    }

    out_str = bson_tree_to_string(&tree);
    if (!out_str)
        BERROR("Failed to serialize heightmap terrain to string. See logs for details");

cleanup_bson:
    bson_tree_cleanup(&tree);

    return out_str;
}

b8 basset_heightmap_terrain_deserialize(const char* file_text, basset* out_asset)
{
    if (out_asset)
    {
        b8 success = false;
        basset_heightmap_terrain* typed_asset = (basset_heightmap_terrain*)out_asset;

        // Deserialize the loaded asset data
        bson_tree tree = {0};
        if (!bson_tree_from_string(file_text, &tree))
        {
            BERROR("Failed to parse asset data for heightmap terrain. See logs for details");
            goto cleanup_bson;
        }

        // version
        if (!bson_object_property_value_get_int(&tree.root, "version", (i64*)(&typed_asset->base.meta.version)))
        {
            BERROR("Failed to parse version, which is a required field");
            goto cleanup_bson;
        }

        // heightmap_asset_name
        const char* heightmap_asset_name_str = 0;
        if (!bson_object_property_value_get_string(&tree.root, "heightmap_asset_name", &heightmap_asset_name_str))
        {
            BERROR("Failed to parse heightmap_asset_name, which is a required field");
            goto cleanup_bson;
        }
        typed_asset->heightmap_asset_name = bname_create(heightmap_asset_name_str);
        string_free(heightmap_asset_name_str);

        // chunk_size
        if (!bson_object_property_value_get_int(&tree.root, "chunk_size", (i64*)(&typed_asset->chunk_size)))
        {
            BERROR("Failed to parse chunk_size, which is a required field");
            goto cleanup_bson;
        }

        // tile_scale - optional with default of 1
        if (bson_object_property_value_get_vec3(&tree.root, "tile_scale", &typed_asset->tile_scale))
            typed_asset->tile_scale = vec3_one();

        // Material names array
        bson_array material_names_obj_array = {0};
        if (!bson_object_property_value_get_object(&tree.root, "material_names", &material_names_obj_array))
        {
            BERROR("Failed to parse material_names, which is a required field");
            goto cleanup_bson;
        }

        // Get the number of elements
        if (!bson_array_element_count_get(&material_names_obj_array, (u32*)(&typed_asset->material_count)))
        {
            BERROR("Failed to parse material_names count. Invalid format?");
            goto cleanup_bson;
        }

        // Setup the new array
        typed_asset->material_names = ballocate(sizeof(const char*) * typed_asset->material_count, MEMORY_TAG_ARRAY);
        for (u32 i = 0; i < typed_asset->material_count; ++i)
        {
            const char* mat_name = 0;
            if (!bson_array_element_value_get_string(&material_names_obj_array, i, &mat_name))
            {
                BWARN("Unable to read material name at index %u, using default of '%s' instead", "default_terrain");
                // Take a duplicate since the cleanup code won't know a constant is used here
                typed_asset->material_names[i] = bname_create("default_terrain");
            }
            typed_asset->material_names[i] = bname_create(mat_name);
            string_free(mat_name);
        }

        success = true;
    cleanup_bson:
        bson_tree_cleanup(&tree);
        if (!success)
        {
            if (typed_asset->material_count && typed_asset->material_names)
            {
                bfree(typed_asset->material_names, sizeof(bname) * typed_asset->material_count, MEMORY_TAG_ARRAY);
                typed_asset->material_names = 0;
                typed_asset->material_count = 0;
            }
        }
        return success;
    }

    BERROR("basset_heightmap_terrain_deserialize serializer requires an asset to deserialize to!");
    return false;
}
