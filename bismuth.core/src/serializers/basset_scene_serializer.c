#include "basset_scene_serializer.h"

#include "assets/basset_types.h"

#include "logger.h"
#include "math/bmath.h"
#include "parsers/bson_parser.h"
#include "strings/bstring.h"

// The current scene version
#define BASSET_SCENE_VERSION 2

static b8 serialize_node(basset_scene_node* node, bson_object* node_obj);
static b8 serialize_attachment(basset_scene_node_attachment* attachment, bson_object* attachment_obj);

static b8 deserialize_node(basset* asset, basset_scene_node* node, bson_object* node_obj);
static b8 deserialize_attachment(basset* asset, basset_scene_node_attachment* attachment, bson_object* attachment_obj);

const char* basset_scene_serialize(const basset* asset)
{
    if (!asset)
    {
        BERROR("basset_scene_serialize requires an asset to serialize!");
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
    if (!bson_object_value_add_int(&tree.root, "version", BASSET_SCENE_VERSION))
    {
        BERROR("Failed to add version, which is a required field");
        goto cleanup_bson;
    }

    // Description - optional
    if (typed_asset->description)
        bson_object_value_add_string(&tree.root, "description", typed_asset->description);

    // Nodes array
    bson_array nodes_array = bson_array_create();
    for (u32 i = 0; i < typed_asset->node_count; ++i)
    {
        basset_scene_node* node = &typed_asset->nodes[i];
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

            // Description comes from here, but is still optional
            bson_object_property_value_get_string(&tree.root, "description", &typed_asset->description);
        }

        // Nodes array
        bson_array nodes_obj_array = {0};
        if (!bson_object_property_value_get_object(&tree.root, "nodes", &nodes_obj_array))
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
        typed_asset->nodes = ballocate(sizeof(basset_scene_node) * typed_asset->node_count, MEMORY_TAG_ARRAY);
        for (u32 i = 0; i < typed_asset->node_count; ++i)
        {
            basset_scene_node* node = &typed_asset->nodes[i];
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

    BERROR("basset_scene_deserialize serializer requires an asset to deserialize to!");
    return false;
}

static b8 serialize_attachment(basset_scene_node_attachment* attachment, bson_object* attachment_obj)
{
    const char* attachment_name = attachment->name ? attachment->name : "unnamed-attachment";

    // Name, if it exists
    if (attachment->name)
    {
        if (!bson_object_value_add_string(attachment_obj, "name", attachment->name))
        {
            BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
            return false;
        }
    }

    // Add the type
    const char* type_str = basset_scene_node_attachment_type_strings[attachment->type];
    if (!bson_object_value_add_string(attachment_obj, "type", type_str))
    {
        BERROR("Failed to add 'name' property for attachment '%s'", attachment_name);
        return false;
    }

    // Process based on attachment type
    switch (attachment->type)
    {
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_SKYBOX:
    {
        basset_scene_node_attachment_skybox* typed_attachment = (basset_scene_node_attachment_skybox*)attachment;

        // Cubemap name
        const char* cubemap_name = typed_attachment->cubemap_image_asset_name ? typed_attachment->cubemap_image_asset_name : "default_skybox";
        if (!bson_object_value_add_string(attachment_obj, "cubemap_image_asset_name", cubemap_name))
        {
            BERROR("Failed to add 'cubemap_image_asset_name' property for attachment '%s'", attachment_name);
            return false;
        }
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT:
    {
        basset_scene_node_attachment_directional_light* typed_attachment = (basset_scene_node_attachment_directional_light*)attachment;

        // Color
        if (!bson_object_value_add_vec4(attachment_obj, "color", typed_attachment->color))
        {
            BERROR("Failed to add 'color' property for attachment '%s'", attachment_name);
            return false;
        }

        // Direction
        if (!bson_object_value_add_vec4(attachment_obj, "direction", typed_attachment->direction))
        {
            BERROR("Failed to add 'direction' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_distance
        if (!bson_object_value_add_float(attachment_obj, "shadow_distance", typed_attachment->shadow_distance))
        {
            BERROR("Failed to add 'shadow_distance' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_fade_distance
        if (!bson_object_value_add_float(attachment_obj, "shadow_fade_distance", typed_attachment->shadow_fade_distance))
        {
            BERROR("Failed to add 'shadow_fade_distance' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_split_mult
        if (!bson_object_value_add_float(attachment_obj, "shadow_split_mult", typed_attachment->shadow_split_mult))
        {
            BERROR("Failed to add 'shadow_split_mult' property for attachment '%s'", attachment_name);
            return false;
        }
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT:
    {
        basset_scene_node_attachment_point_light* typed_attachment = (basset_scene_node_attachment_point_light*)attachment;

        // Color
        if (!bson_object_value_add_vec4(attachment_obj, "color", typed_attachment->color))
        {
            BERROR("Failed to add 'color' property for attachment '%s'", attachment_name);
            return false;
        }

        // Position
        if (!bson_object_value_add_vec4(attachment_obj, "position", typed_attachment->position))
        {
            BERROR("Failed to add 'position' property for attachment '%s'", attachment_name);
            return false;
        }

        // Constant
        if (!bson_object_value_add_float(attachment_obj, "constant_f", typed_attachment->constant_f))
        {
            BERROR("Failed to add 'constant_f' property for attachment '%s'", attachment_name);
            return false;
        }

        // Linear
        if (!bson_object_value_add_float(attachment_obj, "linear", typed_attachment->linear))
        {
            BERROR("Failed to add 'linear' property for attachment '%s'", attachment_name);
            return false;
        }

        // Quadratic
        if (!bson_object_value_add_float(attachment_obj, "quadratic", typed_attachment->quadratic))
        {
            BERROR("Failed to add 'quadratic' property for attachment '%s'", attachment_name);
            return false;
        }
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH:
    {
        basset_scene_node_attachment_static_mesh* typed_attachment = (basset_scene_node_attachment_static_mesh*)attachment;

        // Asset name
        const char* asset_name = typed_attachment->asset_name;
        if (!asset_name)
        {
            BWARN("Attempted to load static mesh (name: '%s') without an asset name. A default mesh name will be used", attachment_name);
            asset_name = "default_static_mesh";
        }
        if (!bson_object_value_add_string(attachment_obj, "asset_name", asset_name))
        {
            BERROR("Failed to add 'cubemap_image_asset_name' property for attachment '%s'", attachment_name);
            return false;
        }
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_HEIGHTMAP_TERRAIN:
    {
        basset_scene_node_attachment_heightmap_terrain* typed_attachment = (basset_scene_node_attachment_heightmap_terrain*)attachment;

        // Asset name
        if (typed_attachment->asset_name)
        {
            if (!bson_object_value_add_string(attachment_obj, "asset_name", typed_attachment->asset_name))
            {
                BERROR("Failed to add 'asset_name' property for attachment '%s'", attachment_name);
                return false;
            }
        }
        else
        {
            BERROR("Cannot add heightmap terrain (name: '%s') without an 'asset_name'!", attachment_name);
            return false;
        }
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE:
    {
        // NOTE: Intentionally blank until additional config is added to water planes
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_COUNT:
        BERROR("Stop trying to serialize the count member of the enum");
        return false;
    }

    return true;
}

static b8 serialize_node(basset_scene_node* node, bson_object* node_obj)
{
    const char* node_name = node->name ? node->name : "unnamed-node";
    // Properties

    // Name, if it exists
    if (node->name)
    {
        if (!bson_object_value_add_string(node_obj, "name", node->name))
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

    // Process attachments
    if (node->attachment_count && node->attachments)
    {
        bson_array attachment_array = bson_array_create();
        for (u32 i = 0; i < node->attachment_count; ++i)
        {
            basset_scene_node_attachment* attachment = &node->attachments[i];
            bson_object attachment_obj = bson_object_create();

            // Process attachment
            if (!serialize_attachment(attachment, &attachment_obj))
            {
                BERROR("Failed to serialize attachment of node '%s'", node_name);
                bson_object_cleanup(&attachment_array);
                return false;
            }

            // Add it to the array
            if (!bson_array_value_add_object(&attachment_array, attachment_obj))
            {
                BERROR("Failed to add attachment to node '%s'", node_name);
                bson_object_cleanup(&attachment_array);
                return false;
            }
        }

        // Add the attachments array to the parent node object
        if (!bson_object_value_add_array(node_obj, "attachments", attachment_array))
        {
            BERROR("Failed to add attachments array to node '%s'", node_name);
            bson_object_cleanup(&attachment_array);
            return false;
        }
    }

    // Process children if there are any
    if (node->child_count && node->children)
    {
        bson_array children_array = bson_array_create();
        for (u32 i = 0; i < node->child_count; ++i)
        {
            basset_scene_node* child = &node->children[i];
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

static b8 deserialize_node(basset* asset, basset_scene_node* node, bson_object* node_obj)
{
    // Get name, if defined. Not required
    bson_object_property_value_get_string(node_obj, "name", &node->name);

    // Get Xform as a string, if it exists. Optional
    bson_object_property_value_get_string(node_obj, "xform", &node->xform_source);

    // Process attachments if there are any. These are optional
    bson_array attachment_obj_array = {0};
    if (bson_object_property_value_get_object(node_obj, "attachments", &attachment_obj_array))
    {
        // Get the number of attachments
        if (!bson_array_element_count_get(&attachment_obj_array, (u32*)(&node->attachment_count)))
        {
            BERROR("Failed to parse attachment count. Invalid format?");
            return false;
        }

        // Setup the attachment array and deserialize
        node->attachments = ballocate(sizeof(basset_scene_node_attachment) * node->attachment_count, MEMORY_TAG_ARRAY);
        for (u32 i = 0; i < node->attachment_count; ++i)
        {
            basset_scene_node_attachment* attachment = &node->attachments[i];
            bson_object attachment_obj;
            if (!bson_array_element_value_get_object(&attachment_obj_array, i, &attachment_obj))
            {
                BWARN("Unable to read attachment at index %u. Skipping...", i);
                continue;
            }

            // Deserialize attachment
            if (!deserialize_attachment(asset, attachment, &attachment_obj))
            {
                BERROR("Failed to deserialize attachment at index %u. Skipping...", i);
                continue;
            }
        }
    }

    // Process children if there are any. These are optional
    bson_array children_obj_array = {0};
    if (bson_object_property_value_get_object(node_obj, "children", &children_obj_array))
    {
        // Get the number of nodes
        if (!bson_array_element_count_get(&children_obj_array, (u32*)(&node->child_count)))
        {
            BERROR("Failed to parse children count. Invalid format?");
            return false;
        }

        // Setup the child array and deserialize
        node->children = ballocate(sizeof(basset_scene_node) * node->child_count, MEMORY_TAG_ARRAY);
        for (u32 i = 0; i < node->child_count; ++i)
        {
            basset_scene_node* child = &node->children[i];
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

static b8 deserialize_attachment(basset* asset, basset_scene_node_attachment* attachment, bson_object* attachment_obj)
{
    // Name, if it exists. Optional
    bson_object_property_value_get_string(attachment_obj, "name", &attachment->name);

    const char* attachment_name = attachment->name ? attachment->name : "unnamed-attachment";

    // Parse the type
    const char* type_str = 0; // basset_scene_node_attachment_type_strings[attachment->type];
    if (!bson_object_property_value_get_string(attachment_obj, "type", &type_str))
    {
        BERROR("Failed to parse required 'type' property for attachment '%s'", attachment_name);
        return false;
    }

    // Find the attachment type
    b8 type_found = false;
    for (u32 i = 0; i < BASSET_SCENE_NODE_ATTACHMENT_TYPE_COUNT; ++i)
    {
        if (strings_equali(basset_scene_node_attachment_type_strings[i], type_str))
        {
            attachment->type = i;
            type_found = true;
            break;
        }

        // Some things in version 1 were named differently. Try those as well if v1
        if (asset->meta.version == 1)
        {
            // fallback types
            if (i == BASSET_SCENE_NODE_ATTACHMENT_TYPE_HEIGHTMAP_TERRAIN)
            {
                if (strings_equali("terrain", type_str))
                {
                    attachment->type = i;
                    type_found = true;
                    break;
                }
            }
        }
    }
    if (!type_found)
    {
        BERROR("Unrecognized attachment type '%s'. Attachment deserialization failed", type_str);
        return false;
    }

    // Process based on attachment type
    switch (attachment->type)
    {
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_SKYBOX:
    {
        basset_scene_node_attachment_skybox* typed_attachment = (basset_scene_node_attachment_skybox*)attachment;

        // Cubemap name
        if (!bson_object_property_value_get_string(attachment_obj, "cubemap_image_asset_name", &typed_attachment->cubemap_image_asset_name))
        {
            // Try fallback name if v1
            if (asset->meta.version == 1)
            {
                if (!bson_object_property_value_get_string(attachment_obj, "cubemap_name", &typed_attachment->cubemap_image_asset_name))
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
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT:
    {
        basset_scene_node_attachment_directional_light* typed_attachment = (basset_scene_node_attachment_directional_light*)attachment;

        // Color
        if (!bson_object_property_value_get_vec4(attachment_obj, "color", &typed_attachment->color))
        {
            BERROR("Failed to get 'color' property for attachment '%s'", attachment_name);
            return false;
        }

        // Direction
        if (!bson_object_property_value_get_vec4(attachment_obj, "direction", &typed_attachment->direction))
        {
            BERROR("Failed to get 'direction' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_distance
        if (!bson_object_property_value_get_float(attachment_obj, "shadow_distance", &typed_attachment->shadow_distance))
        {
            BERROR("Failed to get 'shadow_distance' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_fade_distance
        if (!bson_object_property_value_get_float(attachment_obj, "shadow_fade_distance", &typed_attachment->shadow_fade_distance))
        {
            BERROR("Failed to get 'shadow_fade_distance' property for attachment '%s'", attachment_name);
            return false;
        }

        // shadow_split_mult
        if (!bson_object_property_value_get_float(attachment_obj, "shadow_split_mult", &typed_attachment->shadow_split_mult))
        {
            BERROR("Failed to get 'shadow_split_mult' property for attachment '%s'", attachment_name);
            return false;
        }
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT:
    {
        basset_scene_node_attachment_point_light* typed_attachment = (basset_scene_node_attachment_point_light*)attachment;

        // Color
        if (!bson_object_property_value_get_vec4(attachment_obj, "color", &typed_attachment->color))
        {
            BERROR("Failed to get 'color' property for attachment '%s'", attachment_name);
            return false;
        }

        // Position
        if (!bson_object_property_value_get_vec4(attachment_obj, "position", &typed_attachment->position))
        {
            BERROR("Failed to get 'position' property for attachment '%s'", attachment_name);
            return false;
        }

        // Constant
        if (!bson_object_property_value_get_float(attachment_obj, "constant_f", &typed_attachment->constant_f))
        {
            BERROR("Failed to get 'constant_f' property for attachment '%s'", attachment_name);
            return false;
        }

        // Linear
        if (!bson_object_property_value_get_float(attachment_obj, "linear", &typed_attachment->linear))
        {
            BERROR("Failed to get 'linear' property for attachment '%s'", attachment_name);
            return false;
        }

        // Quadratic
        if (!bson_object_property_value_get_float(attachment_obj, "quadratic", &typed_attachment->quadratic))
        {
            BERROR("Failed to get 'quadratic' property for attachment '%s'", attachment_name);
            return false;
        }
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH:
    {
        basset_scene_node_attachment_static_mesh* typed_attachment = (basset_scene_node_attachment_static_mesh*)attachment;

        // Asset name
        if (!bson_object_property_value_get_string(attachment_obj, "asset_name", &typed_attachment->asset_name))
        {
            // Try fallback
            if (asset->meta.version == 1)
            {
                if (!bson_object_property_value_get_string(attachment_obj, "resource_name", &typed_attachment->asset_name))
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
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_HEIGHTMAP_TERRAIN:
    {
        basset_scene_node_attachment_heightmap_terrain* typed_attachment = (basset_scene_node_attachment_heightmap_terrain*)attachment;

        // Asset name
        if (!bson_object_property_value_get_string(attachment_obj, "asset_name", &typed_attachment->asset_name))
        {
            // Try fallback
            if (asset->meta.version == 1)
            {
                if (!bson_object_property_value_get_string(attachment_obj, "resource_name", &typed_attachment->asset_name))
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
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE:
    {
        // NOTE: Intentionally blank until additional config is added to water planes
    } break;
    case BASSET_SCENE_NODE_ATTACHMENT_TYPE_COUNT:
        BERROR("Stop trying to serialize the count member of the enum");
        return false;
    }

    return true;
}
