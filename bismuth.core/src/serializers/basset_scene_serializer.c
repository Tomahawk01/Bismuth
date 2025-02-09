#include "basset_scene_serializer.h"

#include "assets/basset_types.h"
#include "containers/darray.h"
#include "core_audio_types.h"
#include "core_physics_types.h"
#include "core_resource_types.h"
#include "defines.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "strings/bname.h"
#include "strings/bstring.h"

// The current scene version
#define SCENE_ASSET_CURRENT_VERSION 2

static b8 serialize_node(scene_node_config* node, bson_object* node_obj);

static b8 deserialize_node(basset* asset, scene_node_config* node, bson_object* node_obj);
static b8 deserialize_attachment(basset* asset, scene_node_config* node, bson_object* attachment_obj);

const char* basset_scene_serialize(const basset* asset)
{
    if (!asset)
    {
        BERROR("scene_serialize requires an asset to serialize!");
        BERROR("Scene serialization failed. See logs for details");
        return 0;
    }

    basset_scene* typed_asset = (basset_scene*)asset;
    b8 success = false;
    const char* out_str = 0;

    // Setup the BSON tree to serialize below
    bson_tree tree = {0};
    tree.root = bson_object_create();

    // version - always write the current version
    if (!bson_object_value_add_int(&tree.root, "version", SCENE_ASSET_CURRENT_VERSION))
    {
        BERROR("Failed to add version, which is a required field");
        goto cleanup_bson;
    }

    // Description - optional
    if (typed_asset->description)
        bson_object_value_add_string(&tree.root, "description", typed_asset->description);

    // Physics settings
    bson_object physics_obj = bson_object_create();
    if (!bson_object_value_add_vec3(&physics_obj, "gravity", typed_asset->physics_gravity))
    {
        BERROR("Failed to serialize physics 'gravity' setting");
        goto cleanup_bson;
    }
    if (!bson_object_value_add_boolean(&physics_obj, "enabled", typed_asset->physics_enabled))
    {
        BERROR("Failed to serialize physic 'enabled' setting");
        goto cleanup_bson;
    }
    if (!bson_object_value_add_object(&physics_obj, "physics", physics_obj))
    {
        BERROR("Failed to serialize physics settings");
        goto cleanup_bson;
    }

    // Nodes array
    bson_array nodes_array = bson_array_create();
    for (u32 i = 0; i < typed_asset->node_count; ++i)
    {
        scene_node_config* node = &typed_asset->nodes[i];
        bson_object node_obj = bson_object_create();

        // Serialize the node. This is recursive, and also handles attachments
        if (!serialize_node(node, &node_obj))
        {
            BERROR("Failed to serialize root node '%s'", node->name);
            bson_object_cleanup(&nodes_array);
            goto cleanup_bson;
        }

        // Add the object to the nodes array
        if (!bson_array_value_add_object(&nodes_array, node_obj))
        {
            BERROR("Failed to add child to children array to node '%s'", node->name);
            bson_object_cleanup(&nodes_array);
            goto cleanup_bson;
        }
    }

    // Add the nodes array to the root object
    if (!bson_object_value_add_array(&tree.root, "nodes", nodes_array))
    {
        BERROR("Failed to add nodes, which is a required field");
        bson_object_cleanup(&nodes_array);
        goto cleanup_bson;
    }

    // Serialize the entire thing to string now
    out_str = bson_tree_to_string(&tree);
    if (!out_str)
        BERROR("Failed to serialize scene to string. See logs for details.");

    success = true;
cleanup_bson:
    if (!success)
        BERROR("Scene serialization failed. See logs for details.");
    bson_tree_cleanup(&tree);

    return out_str;
}

b8 basset_scene_deserialize(const char* file_text, basset* out_asset)
{
    if (out_asset)
    {
        b8 success = false;
        basset_scene* typed_asset = (basset_scene*)out_asset;

        // Deserialize the loaded asset data
        bson_tree tree = {0};
        if (!bson_tree_from_string(file_text, &tree))
        {
            BERROR("Failed to parse asset data for scene. See logs for details");
            goto cleanup_bson;
        }

        // Determine the asset version first. Version 1 has a top-level "properties" object that was removed in v2+.
        // Also v1 does not list a version number, whereas v2+ does
        bson_object properties_obj = {0};
        if (bson_object_property_value_get_object(&tree.root, "properties", &properties_obj))
        {
            // This is a version 1 file
            out_asset->meta.version = 1;

            // Description is also extracted from here for v1. This is optional, however, so don't bother checking the result
            bson_object_property_value_get_string(&properties_obj, "description", &typed_asset->description);

            // NOTE: v1 files also had a "name", but this will be ignored in favour of the asset name itself
        }
        else
        {
            // File is v2+, extract the version and description from the root node

            // version is required in this case
            if (!bson_object_property_value_get_int(&tree.root, "version", (i64*)(&typed_asset->base.meta.version)))
            {
                BERROR("Failed to parse version, which is a required field");
                goto cleanup_bson;
            }

            if (typed_asset->base.meta.version > SCENE_ASSET_CURRENT_VERSION)
            {
                BERROR("Parsed scene version '%u' is beyond what the current version '%u' is. Check file format. Deserialization failed", typed_asset->base.meta.version, SCENE_ASSET_CURRENT_VERSION);
                return false;
            }

            // Description comes from here, but is still optional
            bson_object_property_value_get_string(&tree.root, "description", &typed_asset->description);
        }

        // Physics settings, if they exist
        bson_object physics_obj = {0};
        if (bson_object_property_value_get_object(&tree.root, "physics", &physics_obj))
        {
            // Enabled - optional, default = false
            if (!bson_object_property_value_get_bool(&physics_obj, "enabled", &typed_asset->physics_enabled))
            {
                BWARN("Scene parsing found a 'physics' block, but no 'enabled' was defined. Physics will be disabled for this scene");
                typed_asset->physics_enabled = false;
            }

            // Gravity
            if (!bson_object_property_value_get_vec3(&physics_obj, "gravity", &typed_asset->physics_gravity))
            {
                BWARN("Scene parsing found a 'physics' block, but no 'gravity' was defined. Using a reasonable default value");
                typed_asset->physics_gravity = (vec3){0, -9.8f, 0};
            }
        }
        else
        {
            // Physics not defined, set zero gravity
            typed_asset->physics_gravity = vec3_zero();
            typed_asset->physics_enabled = false;
        }

        // Nodes array
        bson_array nodes_obj_array = {0};
        if (!bson_object_property_value_get_array(&tree.root, "nodes", &nodes_obj_array))
        {
            BERROR("Failed to parse nodes, which is a required field");
            goto cleanup_bson;
        }

        // Get the number of nodes
        if (!bson_array_element_count_get(&nodes_obj_array, (u32*)(&typed_asset->node_count)))
        {
            BERROR("Failed to parse node count. Invalid format?");
            goto cleanup_bson;
        }

        // Process nodes
        typed_asset->nodes = BALLOC_TYPE_CARRAY(scene_node_config, typed_asset->node_count);
        for (u32 i = 0; i < typed_asset->node_count; ++i)
        {
            scene_node_config* node = &typed_asset->nodes[i];
            bson_object node_obj;
            if (!bson_array_element_value_get_object(&nodes_obj_array, i, &node_obj))
            {
                BWARN("Unable to read root node at index %u. Skipping...", i);
                continue;
            }

            // Deserialize recursively
            if (!deserialize_node(out_asset, node, &node_obj))
            {
                BERROR("Unable to deserialize root node at index %u. Skipping...", i);
                continue;
            }
        }

        success = true;
    cleanup_bson:
        bson_tree_cleanup(&tree);
        return success;
    }

    BERROR("scene_deserialize serializer requires an asset to deserialize to!");
    return false;
}

static b8 serialize_node(scene_node_config* node, bson_object* node_obj)
{
    bname node_name = node->name ? node->name : bname_create("unnamed-node");
    // Properties

    // Name, if it exists
    if (node->name)
    {
        if (!bson_object_value_add_bname_as_string(node_obj, "name", node->name))
        {
            BERROR("Failed to add 'name' property for node '%s'", node_name);
            return false;
        }
    }

    // Xform as a string, if it exists
    if (node->xform_source)
    {
        if (!bson_object_value_add_string(node_obj, "xform", node->xform_source))
        {
            BERROR("Failed to add 'xform' property for node '%s'", node_name);
            return false;
        }
    }

    // Process attachments by type, but place them all into the same array in the output file
    bson_array attachment_obj_array = bson_array_create();

    // User-defined configs are output directly as-is, not processed here
    if (node->user_defined_configs)
    {
        u32 length = darray_length(node->user_defined_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_user_defined_config* typed_attachment = &node->user_defined_configs[i];
            /* scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment; */

            // Deserialize config source to a temp BSON tree
            bson_tree temp = {0};
            bson_tree_from_string(typed_attachment->config_source, &temp);

            // Add that tree's root as the attachment object
            bson_array_value_add_object(&attachment_obj_array, temp.root);

            // Cleanup the temp tree
            bson_tree_cleanup(&temp);
        }
    }

    if (node->skybox_configs)
    {
        u32 length = darray_length(node->skybox_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_skybox_config* typed_attachment = &node->skybox_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);
            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }

                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Cubemap name
            bname cubemap_name = typed_attachment->cubemap_image_asset_name ? typed_attachment->cubemap_image_asset_name : bname_create("default_skybox");
            if (!bson_object_value_add_bname_as_string(&attachment_obj, "cubemap_image_asset_name", cubemap_name))
            {
                BERROR("Failed to add 'cubemap_image_asset_name' property for attachment '%s'", attachment_name);
                return false;
            }

            // Package name, if it exists
            if (typed_attachment->cubemap_image_asset_package_name)
            {
                if (!bson_object_value_add_bname_as_string(&attachment_obj, "package_name", typed_attachment->cubemap_image_asset_package_name))
                {
                    BERROR("Failed to add 'package_name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Add it to the attachments array
            bson_array_value_add_object(&attachment_obj_array, attachment_obj);
        }
    }

    if (node->dir_light_configs)
    {
        u32 length = darray_length(node->dir_light_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_directional_light_config* typed_attachment = &node->dir_light_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);

            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }

                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Color
            if (!bson_object_value_add_vec4(&attachment_obj, "color", typed_attachment->color))
            {
                BERROR("Failed to add 'color' property for attachment '%s'", attachment_name);
                return false;
            }

            // Direction
            if (!bson_object_value_add_vec4(&attachment_obj, "direction", typed_attachment->direction))
            {
                BERROR("Failed to add 'direction' property for attachment '%s'", attachment_name);
                return false;
            }

            // shadow_distance
            if (!bson_object_value_add_float(&attachment_obj, "shadow_distance", typed_attachment->shadow_distance))
            {
                BERROR("Failed to add 'shadow_distance' property for attachment '%s'", attachment_name);
                return false;
            }

            // shadow_fade_distance
            if (!bson_object_value_add_float(&attachment_obj, "shadow_fade_distance", typed_attachment->shadow_fade_distance))
            {
                BERROR("Failed to add 'shadow_fade_distance' property for attachment '%s'", attachment_name);
                return false;
            }

            // shadow_split_mult
            if (!bson_object_value_add_float(&attachment_obj, "shadow_split_mult", typed_attachment->shadow_split_mult))
            {
                BERROR("Failed to add 'shadow_split_mult' property for attachment '%s'", attachment_name);
                return false;
            }

            // Add it to the attachments array
            bson_array_value_add_object(&attachment_obj_array, attachment_obj);
        }
    }

    if (node->point_light_configs)
    {
        u32 length = darray_length(node->point_light_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_point_light_config* typed_attachment = &node->point_light_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);

            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }
                
                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Color
            if (!bson_object_value_add_vec4(&attachment_obj, "color", typed_attachment->color))
            {
                BERROR("Failed to add 'color' property for attachment '%s'", attachment_name);
                return false;
            }

            // Position
            if (!bson_object_value_add_vec4(&attachment_obj, "position", typed_attachment->position))
            {
                BERROR("Failed to add 'position' property for attachment '%s'", attachment_name);
                return false;
            }

            // Constant
            if (!bson_object_value_add_float(&attachment_obj, "constant_f", typed_attachment->constant_f))
            {
                BERROR("Failed to add 'constant_f' property for attachment '%s'", attachment_name);
                return false;
            }
        
            // Linear
            if (!bson_object_value_add_float(&attachment_obj, "linear", typed_attachment->linear))
            {
                BERROR("Failed to add 'linear' property for attachment '%s'", attachment_name);
                return false;
            }

            // Quadratic
            if (!bson_object_value_add_float(&attachment_obj, "quadratic", typed_attachment->quadratic))
            {
                BERROR("Failed to add 'quadratic' property for attachment '%s'", attachment_name);
                return false;
            }

            // Add it to the attachments array
            bson_array_value_add_object(&attachment_obj_array, attachment_obj);
        }
    }

    if (node->audio_emitter_configs)
    {
        u32 length = darray_length(node->audio_emitter_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_audio_emitter_config* typed_attachment = &node->audio_emitter_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);

            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }

                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // volume
            if (!bson_object_value_add_float(&attachment_obj, "volume", typed_attachment->volume))
            {
                BERROR("Failed to add 'volume' property for attachment '%s'", attachment_name);
                return false;
            }

            // is_looping
            if (!bson_object_value_add_boolean(&attachment_obj, "is_looping", typed_attachment->is_looping))
            {
                BERROR("Failed to add 'is_looping' property for attachment '%s'", attachment_name);
                return false;
            }

            // inner_radius
            if (!bson_object_value_add_float(&attachment_obj, "inner_radius", typed_attachment->inner_radius))
            {
                BERROR("Failed to add 'inner_radius' property for attachment '%s'", attachment_name);
                return false;
            }

            // outer_radius
            if (!bson_object_value_add_float(&attachment_obj, "outer_radius", typed_attachment->outer_radius))
            {
                BERROR("Failed to add 'outer_radius' property for attachment '%s'", attachment_name);
                return false;
            }

            // falloff
            if (!bson_object_value_add_float(&attachment_obj, "falloff", typed_attachment->falloff))
            {
                BERROR("Failed to add 'falloff' property for attachment '%s'", attachment_name);
                return false;
            }

            // is_streaming
            if (!bson_object_value_add_boolean(&attachment_obj, "is_streaming", typed_attachment->is_streaming))
            {
                BERROR("Failed to add 'is_streaming' property for attachment '%s'", attachment_name);
                return false;
            }

            // audio_resource_name
            if (!bson_object_value_add_bname_as_string(&attachment_obj, "audio_resource_name", typed_attachment->audio_resource_name))
            {
                BERROR("Failed to add 'audio_resource_name' property for attachment '%s'", attachment_name);
                return false;
            }

            // audio_resource_package_name
            if (!bson_object_value_add_bname_as_string(&attachment_obj, "audio_resource_package_name", typed_attachment->audio_resource_package_name))
            {
                BERROR("Failed to add 'audio_resource_package_name' property for attachment '%s'", attachment_name);
                return false;
            }

            // Add it to the attachments array
            bson_array_value_add_object(&attachment_obj_array, attachment_obj);
        }
    }

    if (node->static_mesh_configs)
    {
        u32 length = darray_length(node->static_mesh_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_static_mesh_config* typed_attachment = &node->static_mesh_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);

            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }

                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Asset name
            bname cubemap_name = typed_attachment->asset_name ? typed_attachment->asset_name : bname_create("default_static_mesh");
            if (!bson_object_value_add_bname_as_string(&attachment_obj, "asset_name", cubemap_name))
            {
                BERROR("Failed to add 'asset_name' property for attachment '%s'", attachment_name);
                return false;
            }

            // Package name, if it exists
            if (typed_attachment->package_name)
            {
                if (!bson_object_value_add_bname_as_string(&attachment_obj, "package_name", typed_attachment->package_name))
                {
                    BERROR("Failed to add 'package_name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Add it to the attachments array
            bson_array_value_add_object(&attachment_obj_array, attachment_obj);
        }
    }

    if (node->heightmap_terrain_configs)
    {
        u32 length = darray_length(node->heightmap_terrain_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_heightmap_terrain_config* typed_attachment = &node->heightmap_terrain_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);

            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }

                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Asset name
            bname cubemap_name = typed_attachment->asset_name ? typed_attachment->asset_name : bname_create("default_terrain");
            if (!bson_object_value_add_bname_as_string(&attachment_obj, "asset_name", cubemap_name))
            {
                BERROR("Failed to add 'asset_name' property for attachment '%s'", attachment_name);
                return false;
            }

            // Package name, if it exists
            if (typed_attachment->package_name)
            {
                if (!bson_object_value_add_bname_as_string(&attachment_obj, "package_name", typed_attachment->package_name))
                {
                    BERROR("Failed to add 'package_name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Add it to the attachments array
            bson_array_value_add_object(&attachment_obj_array, attachment_obj);
        }
    }
    
    if (node->water_plane_configs)
    {
        u32 length = darray_length(node->water_plane_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_water_plane_config* typed_attachment = &node->water_plane_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);

            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }

                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // NOTE: No extra properties for now until additional config is added to water planes

            // Add it to the attachments array
            bson_array_value_add_object(&attachment_obj_array, attachment_obj);
        }
    }

    if (node->physics_body_configs)
    {
        u32 length = darray_length(node->physics_body_configs);
        for (u32 i = 0; i < length; ++i)
        {
            scene_node_attachment_physics_body_config* typed_attachment = &node->physics_body_configs[i];
            scene_node_attachment_config* attachment = (scene_node_attachment_config*)typed_attachment;
            bson_object attachment_obj = bson_object_create();
            const char* attachment_name = bname_string_get(attachment->name);

            // Base properties
            {
                // Name, if it exists
                if (attachment->name)
                {
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "name", attachment->name))
                    {
                        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                        return false;
                    }
                }

                // Add the type. Required
                const char* type_str = scene_node_attachment_type_strings[attachment->type];
                if (!bson_object_value_add_string(&attachment_obj, "type", type_str))
                {
                    BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Specific properties
            // Mass
            {
                if (!bson_object_value_add_float(&attachment_obj, "mass", typed_attachment->mass))
                {
                    BERROR("Failed to add 'mass' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Inertia
            {
                if (!bson_object_value_add_float(&attachment_obj, "inertia", typed_attachment->inertia))
                {
                    BERROR("Failed to add 'inertia' property for attachment '%s'", attachment_name);
                    return false;
                }
            }

            // Shape type
            {
                char* shape_type_str = 0;
                switch (typed_attachment->shape_type)
                {
                case BPHYSICS_SHAPE_TYPE_SPHERE:
                    shape_type_str = "sphere";
                    break;
                case BPHYSICS_SHAPE_TYPE_RECTANGLE:
                    shape_type_str = "rectangle";
                    break;
                case BPHYSICS_SHAPE_TYPE_MESH:
                    shape_type_str = "mesh";
                    break;
                }

                if (!bson_object_value_add_string(&attachment_obj, "body_type", shape_type_str))
                {
                    BERROR("Failed to add 'body_type' property for attachment '%s'", attachment_name);
                    return false;
                }

                // Only doing this a second time so these are added in a resonable order
                switch (typed_attachment->shape_type)
                {
                case BPHYSICS_SHAPE_TYPE_SPHERE:
                    if (!bson_object_value_add_float(&attachment_obj, "radius", typed_attachment->radius))
                    {
                        BERROR("Failed to add property 'radius' to attachment '%s'", attachment_name);
                        return false;
                    }
                    break;
                case BPHYSICS_SHAPE_TYPE_RECTANGLE:
                    if (!bson_object_value_add_vec3(&attachment_obj, "extents", typed_attachment->extents))
                    {
                        BERROR("Failed to add property 'radius' to attachment '%s'", attachment_name);
                        return false;
                    }
                    break;
                case BPHYSICS_SHAPE_TYPE_MESH:
                    if (!bson_object_value_add_bname_as_string(&attachment_obj, "mesh_resource_name", typed_attachment->mesh_resource_name))
                    {
                        BERROR("Failed to add property 'mesh_resource_name' to attachment '%s'", attachment_name);
                        return false;
                    }
                    break;
                }
            }
        }
    }

    // Only write out the attachments array object if it contains something
    u32 total_attachment_count = 0;
    bson_array_element_count_get(&attachment_obj_array, &total_attachment_count);
    if (total_attachment_count > 0)
    {
        // Add the attachments array to the parent node object
        if (!bson_object_value_add_array(node_obj, "attachments", attachment_obj_array))
        {
            BERROR("Failed to add attachments array to node '%s'", node_name);
            bson_object_cleanup(&attachment_obj_array);
            return false;
        }
    }
    else
    {
        bson_object_cleanup(&attachment_obj_array);
    }

    // Process children if there are any
    if (node->child_count && node->children)
    {
        bson_array children_array = bson_array_create();
        for (u32 i = 0; i < node->child_count; ++i)
        {
            scene_node_config* child = &node->children[i];
            bson_object child_obj = bson_object_create();

            // Recurse
            if (!serialize_node(child, &child_obj))
            {
                BERROR("Failed to serialize child node of node '%s'", node_name);
                bson_object_cleanup(&children_array);
                return false;
            }

            // Add it to the array
            if (!bson_array_value_add_object(&children_array, child_obj))
            {
                BERROR("Failed to add child to children array to node '%s'", node_name);
                bson_object_cleanup(&children_array);
                return false;
            }
        }

        // Add the children array to the parent node object
        if (!bson_object_value_add_array(node_obj, "children", children_array))
        {
            BERROR("Failed to add children array to node '%s'", node_name);
            bson_object_cleanup(&children_array);
            return false;
        }
    }

    return true;
}

static b8 deserialize_node(basset* asset, scene_node_config* node, bson_object* node_obj)
{
    // Get name, if defined. Not required
    bson_object_property_value_get_string_as_bname(node_obj, "name", &node->name);

    // Get Xform as a string, if it exists. Optional
    bson_object_property_value_get_string(node_obj, "xform", &node->xform_source);

    // Process attachments if there are any. These are optional
    bson_array attachment_obj_array = {0};
    if (bson_object_property_value_get_array(node_obj, "attachments", &attachment_obj_array))
    {
        // Get the number of attachments
        u32 attachment_count;
        if (!bson_array_element_count_get(&attachment_obj_array, (u32*)(&attachment_count)))
        {
            BERROR("Failed to parse attachment count. Invalid format?");
            return false;
        }

        // Setup the attachment array and deserialize
        for (u32 i = 0; i < attachment_count; ++i)
        {
            bson_object attachment_obj;
            if (!bson_array_element_value_get_object(&attachment_obj_array, i, &attachment_obj))
            {
                BWARN("Unable to read attachment at index %u. Skipping...", i);
                continue;
            }

            // Deserialize attachment
            if (!deserialize_attachment(asset, node, &attachment_obj))
            {
                BERROR("Failed to deserialize attachment at index %u. Skipping...", i);
                continue;
            }
        }
    }

    // Process children if there are any. These are optional
    bson_array children_obj_array = {0};
    if (bson_object_property_value_get_array(node_obj, "children", &children_obj_array))
    {
        // Get the number of nodes
        if (!bson_array_element_count_get(&children_obj_array, (u32*)(&node->child_count)))
        {
            BERROR("Failed to parse children count. Invalid format?");
            return false;
        }

        // Setup the child array and deserialize
        node->children = BALLOC_TYPE_CARRAY(scene_node_config, node->child_count);
        for (u32 i = 0; i < node->child_count; ++i)
        {
            scene_node_config* child = &node->children[i];
            bson_object node_obj;
            if (!bson_array_element_value_get_object(&children_obj_array, i, &node_obj))
            {
                BWARN("Unable to read child node at index %u. Skipping...", i);
                continue;
            }

            // Deserialize recursively
            if (!deserialize_node(asset, child, &node_obj))
            {
                BERROR("Unable to deserialize root node at index %u. Skipping...", i);
                continue;
            }
        }
    }

    // Done!
    return true;
}

static b8 deserialize_attachment(basset* asset, scene_node_config* node, bson_object* attachment_obj)
{
    // Name, if it exists. Optional
    bname name = INVALID_BNAME;
    bson_object_property_value_get_string_as_bname(attachment_obj, "name", &name);

    bname attachment_name = name ? name : bname_create("unnamed-attachment");

    // Parse the type
    const char* type_str = 0; // scene_node_attachment_type_strings[attachment->type];
    if (!bson_object_property_value_get_string(attachment_obj, "type", &type_str))
    {
        BERROR("Failed to parse required 'type' property for attachment '%s'", attachment_name);
        return false;
    }

    // Find the attachment type
    scene_node_attachment_type type = SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN;
    for (u32 i = 0; i < SCENE_NODE_ATTACHMENT_TYPE_COUNT; ++i)
    {
        if (strings_equali(scene_node_attachment_type_strings[i], type_str))
        {
            type = (scene_node_attachment_type)i;
            break;
        }

        // Some things in version 1 were named differently. Try those as well if v1
        if (asset->meta.version == 1)
        {
            // fallback types
            if (i == SCENE_NODE_ATTACHMENT_TYPE_HEIGHTMAP_TERRAIN)
            {
                if (strings_equali("terrain", type_str))
                {
                    type = (scene_node_attachment_type)i;
                    break;
                }
            }
        }
    }
    if (type == SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN)
    {
        // Switch to user-defined if no match is found
        type = SCENE_NODE_ATTACHMENT_TYPE_USER_DEFINED;
    }

    // Process based on attachment type
    switch (type)
    {
    case SCENE_NODE_ATTACHMENT_TYPE_USER_DEFINED:
    {
        scene_node_attachment_user_defined_config typed_attachment = {0};

        // Store off the source as a string and deserialize later
        bson_tree temp = {0};
        temp.root = *attachment_obj;
        typed_attachment.config_source = bson_tree_to_string(&temp);

        // Push to the appropriate array
        if (!node->user_defined_configs)
            node->user_defined_configs = darray_create(scene_node_attachment_user_defined_config);
        darray_push(node->user_defined_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN:
    {
        BERROR("Stop trying to deserialize the unknown member of the enum!");
        return false;
    } break;

    case SCENE_NODE_ATTACHMENT_TYPE_SKYBOX:
    {
        scene_node_attachment_skybox_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;

        // Cubemap name
        if (!bson_object_property_value_get_string_as_bname(attachment_obj, "cubemap_image_asset_name", &typed_attachment.cubemap_image_asset_name))
        {
            // Try fallback name if v1
            if (asset->meta.version == 1)
            {
                if (!bson_object_property_value_get_string_as_bname(attachment_obj, "cubemap_name", &typed_attachment.cubemap_image_asset_name))
                {
                    BERROR("Failed to add 'cubemap_name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }
            else
            {
                BERROR("Failed to add 'cubemap_image_asset_name' property for attachment '%s'", attachment_name);
                return false;
            }
        }

        // Package name. Optional
        bson_object_property_value_get_string_as_bname(attachment_obj, "package_name", &typed_attachment.cubemap_image_asset_package_name);

        // Push to the appropriate array
        if (!node->skybox_configs)
            node->skybox_configs = darray_create(scene_node_attachment_skybox_config);
        darray_push(node->skybox_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT:
    {
        scene_node_attachment_directional_light_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;

        // Color
        if (!bson_object_property_value_get_vec4(attachment_obj, "color", &typed_attachment.color))
        {
            BERROR("Failed to get 'color' property for attachment '%s'", attachment_name);
            return false;
        }

        // Direction
        if (!bson_object_property_value_get_vec4(attachment_obj, "direction", &typed_attachment.direction))
        {
            BERROR("Failed to get 'direction' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_distance
        if (!bson_object_property_value_get_float(attachment_obj, "shadow_distance", &typed_attachment.shadow_distance))
        {
            BERROR("Failed to get 'shadow_distance' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_fade_distance
        if (!bson_object_property_value_get_float(attachment_obj, "shadow_fade_distance", &typed_attachment.shadow_fade_distance))
        {
            BERROR("Failed to get 'shadow_fade_distance' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_split_mult
        if (!bson_object_property_value_get_float(attachment_obj, "shadow_split_mult", &typed_attachment.shadow_split_mult))
        {
            BERROR("Failed to get 'shadow_split_mult' property for attachment '%s'", attachment_name);
            return false;
        }

        // Push to the appropriate array
        if (!node->dir_light_configs)
            node->dir_light_configs = darray_create(scene_node_attachment_directional_light_config);
        darray_push(node->dir_light_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT:
    {
        scene_node_attachment_point_light_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;

        // Color
        if (!bson_object_property_value_get_vec4(attachment_obj, "color", &typed_attachment.color))
        {
            BERROR("Failed to get 'color' property for attachment '%s'", attachment_name);
            return false;
        }

        // Position
        if (!bson_object_property_value_get_vec4(attachment_obj, "position", &typed_attachment.position))
        {
            BERROR("Failed to get 'position' property for attachment '%s'", attachment_name);
            return false;
        }

        // Constant
        if (!bson_object_property_value_get_float(attachment_obj, "constant_f", &typed_attachment.constant_f))
        {
            BERROR("Failed to get 'constant_f' property for attachment '%s'", attachment_name);
            return false;
        }

        // Linear
        if (!bson_object_property_value_get_float(attachment_obj, "linear", &typed_attachment.linear))
        {
            BERROR("Failed to get 'linear' property for attachment '%s'", attachment_name);
            return false;
        }

        // Quadratic
        if (!bson_object_property_value_get_float(attachment_obj, "quadratic", &typed_attachment.quadratic))
        {
            BERROR("Failed to get 'quadratic' property for attachment '%s'", attachment_name);
            return false;
        }

        // Push to the appropriate array
        if (!node->point_light_configs)
            node->point_light_configs = darray_create(scene_node_attachment_point_light_config);
        darray_push(node->point_light_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_AUDIO_EMITTER:
    {
        scene_node_attachment_audio_emitter_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;

        // volume - optional
        if (!bson_object_property_value_get_float(attachment_obj, "volume", &typed_attachment.volume))
        {
            typed_attachment.volume = AUDIO_VOLUME_DEFAULT;
        }

        // is_looping - optional
        if (!bson_object_property_value_get_bool(attachment_obj, "is_looping", &typed_attachment.is_looping))
        {
            // Emitters always default to true for looping, if not defined
            typed_attachment.is_looping = true;
        }

        // inner_radius - optional
        if (!bson_object_property_value_get_float(attachment_obj, "inner_radius", &typed_attachment.inner_radius))
        {
            typed_attachment.inner_radius = AUDIO_INNER_RADIUS_DEFAULT;
        }

        // outer_radius - optional
        if (!bson_object_property_value_get_float(attachment_obj, "outer_radius", &typed_attachment.outer_radius))
        {
            typed_attachment.outer_radius = AUDIO_OUTER_RADIUS_DEFAULT;
        }

        // falloff - optional
        if (!bson_object_property_value_get_float(attachment_obj, "falloff", &typed_attachment.falloff))
        {
            typed_attachment.falloff = AUDIO_FALLOFF_DEFAULT;
        }

        // is_streaming - optional - defaults to false
        if (!bson_object_property_value_get_bool(attachment_obj, "is_streaming", &typed_attachment.is_streaming))
        {
            typed_attachment.is_streaming = false;
        }

        // audio_resource_name - required
        if (!bson_object_property_value_get_string_as_bname(attachment_obj, "audio_resource_name", &typed_attachment.audio_resource_name))
        {
            BERROR("Failed to get 'audio_resource_name' property for attachment '%s'", attachment_name);
            return false;
        }

        // audio_resource_package_name - required
        if (!bson_object_property_value_get_string_as_bname(attachment_obj, "audio_resource_package_name", &typed_attachment.audio_resource_package_name))
        {
            BERROR("Failed to get 'audio_resource_package_name' property for attachment '%s'", attachment_name);
            return false;
        }

        // Push to the appropriate array
        if (!node->audio_emitter_configs)
        {
            node->audio_emitter_configs = darray_create(scene_node_attachment_audio_emitter_config);
        }
        darray_push(node->audio_emitter_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH:
    {
        scene_node_attachment_static_mesh_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;

        // Asset name
        if (!bson_object_property_value_get_string_as_bname(attachment_obj, "asset_name", &typed_attachment.asset_name))
        {
            // Try fallback
            if (asset->meta.version == 1)
            {
                if (!bson_object_property_value_get_string_as_bname(attachment_obj, "resource_name", &typed_attachment.asset_name))
                {
                    BERROR("Failed to get 'resource_name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }
            else
            {
                BERROR("Failed to get 'asset_name' property for attachment '%s'", attachment_name);
                return false;
            }
        }

        // Package name. Optional
        bson_object_property_value_get_string_as_bname(attachment_obj, "package_name", &typed_attachment.package_name);

        // Push to the appropriate array
        if (!node->static_mesh_configs)
            node->static_mesh_configs = darray_create(scene_node_attachment_static_mesh_config);
        darray_push(node->static_mesh_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_HEIGHTMAP_TERRAIN:
    {
        scene_node_attachment_heightmap_terrain_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;

        // Asset name
        if (!bson_object_property_value_get_string_as_bname(attachment_obj, "asset_name", &typed_attachment.asset_name))
        {
            // Try fallback
            if (asset->meta.version == 1)
            {
                if (!bson_object_property_value_get_string_as_bname(attachment_obj, "resource_name", &typed_attachment.asset_name))
                {
                    BERROR("Failed to get 'resource_name' property for attachment '%s'", attachment_name);
                    return false;
                }
            }
            else
            {
                BERROR("Failed to get 'asset_name' property for attachment '%s'", attachment_name);
                return false;
            }
        }

        // Package name. Optional
        bson_object_property_value_get_string_as_bname(attachment_obj, "package_name", &typed_attachment.package_name);

        // Push to the appropriate array
        if (!node->heightmap_terrain_configs)
            node->heightmap_terrain_configs = darray_create(scene_node_attachment_heightmap_terrain_config);
        darray_push(node->heightmap_terrain_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE:
    {
        scene_node_attachment_water_plane_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;
        // NOTE: Intentionally blank until additional config is added to water planes

        // Push to the appropriate array
        if (!node->water_plane_configs)
            node->water_plane_configs = darray_create(scene_node_attachment_water_plane_config);
        darray_push(node->water_plane_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_PHYSICS_BODY:
    {
        scene_node_attachment_physics_body_config typed_attachment = {0};
        typed_attachment.base.name = attachment_name;

        // Mass is optional and defaults to 0 (i.e. static body)
        {
            if (!bson_object_property_value_get_float(attachment_obj, "mass", &typed_attachment.mass))
                typed_attachment.mass = 0;
        }

        // Inertia is optional and defaults to 0 (i.e. changes direction sharply)
        {
            if (!bson_object_property_value_get_float(attachment_obj, "inertia", &typed_attachment.inertia))
                typed_attachment.inertia = 0;
        }

        // Shape type is required
        {
            const char* shape_type_str = 0;
            if (!bson_object_property_value_get_string(attachment_obj, "shape_type", &shape_type_str))
            {
                BERROR("Failed to get required 'shape_type' property for attachment '%s'", attachment_name);
                return false;
            }
            if (strings_equali(shape_type_str, "sphere"))
            {
                typed_attachment.shape_type = BPHYSICS_SHAPE_TYPE_SPHERE;
            }
            else if (strings_equali(shape_type_str, "rect") || strings_equali(shape_type_str, "rectangle"))
            {
                typed_attachment.shape_type = BPHYSICS_SHAPE_TYPE_RECTANGLE;
            }
            else if (strings_equali(shape_type_str, "mesh"))
            {
                typed_attachment.shape_type = BPHYSICS_SHAPE_TYPE_MESH;
            }
            else
            {
                BERROR("Unrecognized physics shape type '%s'. Skipping...", shape_type_str);
                return false;
            }
        }

        // Extract required properties based on shape type
        switch (typed_attachment.shape_type)
        {
        case BPHYSICS_SHAPE_TYPE_SPHERE:
        {
            // Sphere just requires radius
            if (!bson_object_property_value_get_float(attachment_obj, "radius", &typed_attachment.radius))
            {
                BERROR("Attachment '%s', of type 'sphere' did not contain required property 'radius'", attachment_name);
                return false;
            }
        } break;
        case BPHYSICS_SHAPE_TYPE_RECTANGLE:
        {
            // Rectangle just requires extents
            if (!bson_object_property_value_get_vec3(attachment_obj, "extents", &typed_attachment.extents))
            {
                BERROR("Attachment '%s', of type 'rectangle' did not contain required property 'extents'", attachment_name);
                return false;
            }
        } break;
        case BPHYSICS_SHAPE_TYPE_MESH:
        {
            // Mesh just requires the name of the mesh resource to load as collision
            if (!bson_object_property_value_get_string_as_bname(attachment_obj, "mesh_resource_name", &typed_attachment.mesh_resource_name))
            {
                BERROR("Attachment '%s', of type 'mesh' did not contain required property 'mesh_resource_name'", attachment_name);
                return false;
            }
        } break;
        }

        // Push to the appropriate array
        if (!node->physics_body_configs)
            node->physics_body_configs = darray_create(scene_node_attachment_physics_body_config);

        darray_push(node->physics_body_configs, typed_attachment);
    } break;
    case SCENE_NODE_ATTACHMENT_TYPE_COUNT:
        BERROR("Stop trying to serialize the count member of the enum");
        return false;
    }

    return true;
}
