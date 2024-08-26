#include "basset_heightmap_terrain_serializer.h"

#include "assets/basset_types.h"

#include "logger.h"
#include "math/bmath.h"
#include "parsers/bson_parser.h"
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
    if (!bson_object_value_add_string(&tree.root, "heightmap_filename", typed_asset->heightmap_filename))
    {
        BERROR("Failed to add heightmap_filename, which is a required field");
        goto cleanup_bson;
    }

    // chunk_size
    if (!bson_object_value_add_int(&tree.root, "chunk_size", typed_asset->chunk_size))
    {
        BERROR("Failed to add chunk_size, which is a required field");
        goto cleanup_bson;
    }

    // tile_scale - vectors are represented as strings, so convert to string then add it
    const char* temp_tile_scale_str = vec3_to_string(typed_asset->tile_scale);
    if (!temp_tile_scale_str)
    {
        BWARN("Failed to convert tile_scale to string, defaulting to scale of 1. Check data");
        temp_tile_scale_str = vec3_to_string(vec3_one());
    }
    if (!bson_object_value_add_string(&tree.root, "tile_scale", temp_tile_scale_str))
    {
        BERROR("Failed to add tile_scale, which is a required field");
        goto cleanup_bson;
    }
    string_free(temp_tile_scale_str);
    temp_tile_scale_str = 0;

    // Material names array
    bson_array material_names_array = bson_array_create();
    for (u32 i = 0; i < typed_asset->material_count; ++i)
    {
        if (!bson_array_value_add_string(&material_names_array, typed_asset->material_names[i]))
        {
            BWARN("Unable to set material name at index %u, using default of '%s' instead", "default_terrain");
            // Take a duplicate since the cleanup code won't know a constant is used here
            typed_asset->material_names[i] = string_duplicate("default_terrain");
        }
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

        // heightmap_filename
        if (!bson_object_property_value_get_string(&tree.root, "heightmap_filename", &typed_asset->heightmap_filename))
        {
            BERROR("Failed to parse heightmap_filename, which is a required field");
            goto cleanup_bson;
        }

        // chunk_size
        if (!bson_object_property_value_get_int(&tree.root, "chunk_size", (i64*)(&typed_asset->chunk_size)))
        {
            BERROR("Failed to parse chunk_size, which is a required field");
            goto cleanup_bson;
        }

        // tile_scale - vectors are represented as strings, so get that then parse it
        const char* temp_tile_scale_str = 0;
        if (!bson_object_property_value_get_string(&tree.root, "tile_scale", &temp_tile_scale_str))
        {
            BERROR("Failed to parse tile_scale, which is a required field");
            goto cleanup_bson;
        }
        if (!string_to_vec3(temp_tile_scale_str, &typed_asset->tile_scale))
        {
            BWARN("Failed to parse tile_scale from string, defaulting to scale of 1. Check file format");
            typed_asset->tile_scale = vec3_one();
        }
        string_free(temp_tile_scale_str);
        temp_tile_scale_str = 0;

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
            if (!bson_array_element_value_get_string(&material_names_obj_array, i, &typed_asset->material_names[i]))
            {
                BWARN("Unable to read material name at index %u, using default of '%s' instead", "default_terrain");
                // Take a duplicate since the cleanup code won't know a constant is used here
                typed_asset->material_names[i] = string_duplicate("default_terrain");
            }
        }

        success = true;
    cleanup_bson:
        bson_tree_cleanup(&tree);
        if (!success)
        {
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
        return success;
    }

    BERROR("basset_heightmap_terrain_deserialize serializer requires an asset to deserialize to!");
    return false;
}
