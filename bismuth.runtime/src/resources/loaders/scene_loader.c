#include "scene_loader.h"

#include "containers/darray.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "platform/filesystem.h"
#include "resources/loaders/loader_utils.h"
#include "resources/resource_types.h"
#include "resources/scene.h"
#include "strings/bstring.h"
#include "systems/resource_system.h"
#include "systems/xform_system.h"

#define SHADOW_DISTANCE_DEFAULT 200.0f
#define SHADOW_FADE_DISTANCE_DEFAULT 25.0f
#define SHADOW_SPLIT_MULT_DEFAULT 0.95f;

static b8 deserialize_scene_directional_light_attachment(const bson_object* attachment_object, scene_node_attachment_directional_light* attachment)
{
    if (!attachment_object || !attachment)
        return false;

    // NOTE: all properties are optional, with defaults provided

    // Color
    attachment->color = vec4_create(50, 50, 50, 1); // default to white
    const char* color_string = 0;
    if (bson_object_property_value_get_string(attachment_object, "color", &color_string))
        string_to_vec4(color_string, &attachment->color);

    // Direction
    attachment->direction = vec4_create(0, -1, 0, 1); // default to down
    const char* direction_string = 0;
    if (bson_object_property_value_get_string(attachment_object, "direction", &direction_string))
        string_to_vec4(direction_string, &attachment->direction);

    // shadow distance
    attachment->shadow_distance = 200.0f;
    bson_object_property_value_get_float(attachment_object, "shadow_distance", &attachment->shadow_distance);

    // shadow fade distance
    attachment->shadow_fade_distance = 25.0f;
    bson_object_property_value_get_float(attachment_object, "shadow_fade_distance", &attachment->shadow_fade_distance);

    // shadow split multiplier
    attachment->shadow_split_mult = 0.44f;
    bson_object_property_value_get_float(attachment_object, "shadow_split_mult", &attachment->shadow_split_mult);

    return true;
}

static b8 deserialize_scene_point_light_attachment(const bson_object* attachment_object, scene_node_attachment_point_light* attachment)
{
    if (!attachment_object || !attachment)
        return false;

    // NOTE: all properties are optional, with defaults provided

    // Color
    attachment->color = vec4_create(50, 50, 50, 1); // default to white
    const char* color_string = 0;
    if (bson_object_property_value_get_string(attachment_object, "color", &color_string))
        string_to_vec4(color_string, &attachment->color);

    // Position
    attachment->position = vec4_zero(); // default to origin
    const char* position_string = 0;
    if (bson_object_property_value_get_string(attachment_object, "position", &position_string))
        string_to_vec4(position_string, &attachment->position);

    // constant_f
    attachment->constant_f = 1.0f;
    bson_object_property_value_get_float(attachment_object, "constant_f", &attachment->constant_f);

    // linear
    attachment->linear = 0.35f;
    bson_object_property_value_get_float(attachment_object, "linear", &attachment->linear);

    // quadratic
    attachment->quadratic = 0.44f;
    bson_object_property_value_get_float(attachment_object, "quadratic", &attachment->quadratic);

    return true;
}

static b8 deserialize_scene_static_mesh_attachment(const bson_object* attachment_object, scene_node_attachment_static_mesh* attachment)
{
    if (!attachment_object || !attachment)
        return false;

    const char* resource_name_str = 0;
    if (!bson_object_property_value_get_string(attachment_object, "resource_name", &resource_name_str))
    {
        BERROR("Static mesh attachment config requires a valid 'resource_name'. Deserialization failed");
        return false;
    }
    attachment->resource_name = string_duplicate(resource_name_str);

    return true;
}

static b8 deserialize_scene_terrain_attachment(const bson_object* attachment_object, scene_node_attachment_terrain* attachment)
{
    if (!attachment_object || !attachment)
        return false;

    const char* name_str = 0;
    if (!bson_object_property_value_get_string(attachment_object, "name", &name_str))
    {
        BERROR("Terrain attachment config requires a valid 'name'. Deserialization failed");
        return false;
    }
    attachment->name = string_duplicate(name_str);

    const char* resource_name_str = 0;
    if (!bson_object_property_value_get_string(attachment_object, "resource_name", &resource_name_str))
    {
        BERROR("Terrain attachment config requires a valid 'resource_name'. Deserialization failed");
        return false;
    }
    attachment->resource_name = string_duplicate(resource_name_str);

    return true;
}

static b8 deserialize_scene_skybox_attachment(const bson_object* attachment_object, scene_node_attachment_skybox* attachment)
{
    if (!attachment_object || !attachment)
        return false;

    const char* cubemap_name_str = 0;
    if (!bson_object_property_value_get_string(attachment_object, "cubemap_name", &cubemap_name_str))
    {
        BERROR("Skybox attachment config requires a valid 'cubemap_name'. Deserialization failed");
        return false;
    }
    attachment->cubemap_name = string_duplicate(cubemap_name_str);

    return true;
}

static b8 deserialize_scene_water_plane_attachment(const bson_object* attachment_object, scene_node_attachment_water_plane* attachment)
{
    if (!attachment_object || !attachment)
        return false;

    // TODO: water plane specific properties
    // const char* cubemap_name_str = 0;
    // if (!bson_object_property_value_get_string(attachment_object, "cubemap_name", &cubemap_name_str))
    // {
    //     BERROR("Skybox attachment config requires a valid 'cubemap_name'. Deserialization failed");
    //     return false;
    // }
    // attachment->cubemap_name = string_duplicate(cubemap_name_str);

    return true;
}

static scene_node_attachment_type scene_attachment_type_from_string(const char* str)
{
    if (!str)
        return SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN;

    if (strings_equali(str, "static_mesh"))
        return SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH;
    else if (strings_equali(str, "terrain"))
        return SCENE_NODE_ATTACHMENT_TYPE_TERRAIN;
    else if (strings_equali(str, "skybox"))
        return SCENE_NODE_ATTACHMENT_TYPE_SKYBOX;
    else if (strings_equali(str, "directional_light"))
        return SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT;
    else if (strings_equali(str, "point_light"))
        return SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT;
    else if (strings_equali(str, "water_plane"))
        return SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE;
    else
        return SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN;
}

b8 scene_node_config_deserialize_bson(const bson_object* node_object, scene_node_config* out_node_config)
{
    if (!node_object)
    {
        BERROR("scene_node_config_deserialize_bson requires a valid pointer to node_object");
        return false;
    }else

    if (!out_node_config)
    {
        BERROR("scene_node_config_deserialize_bson requires a valid pointer to out_node_config");
        return false;
    }

    if (node_object->type != BSON_OBJECT_TYPE_OBJECT)
    {
        BERROR("Unexpected property type. Skipping");
        return false;
    }

    // Name
    const char* node_name = 0;
    if (!bson_object_property_value_get_string(node_object, "name", &node_name))
        node_name = "";
    out_node_config->name = string_duplicate(node_name);

    // xform, if there is one
    const char* xform_string = 0;
    if (bson_object_property_value_get_string(node_object, "xform", &xform_string))
    {
        // Found an xform, deserialize it into config
        out_node_config->xform = ballocate(sizeof(scene_xform_config), MEMORY_TAG_SCENE);
        string_to_scene_xform_config(xform_string, out_node_config->xform);
    }
    else
    {
        out_node_config->xform = 0;
    }

    // Process attachments, if any
    bson_object attachments_array = {0};
    if (bson_object_property_value_get_object(node_object, "attachments", &attachments_array))
    {
        // Make sure it is actually an array
        if (attachments_array.type == BSON_OBJECT_TYPE_ARRAY)
        {
            u32 attachment_count = 0;
            bson_array_element_count_get(&attachments_array, &attachment_count);

            // Each attachment
            for (u32 attachment_index = 0; attachment_index < attachment_count; ++attachment_index)
            {
                // Get the object
                bson_object attachment_object = {0};
                if (!bson_array_element_value_get_object(&attachments_array, attachment_index, &attachment_object))
                {
                    BERROR("Unable to get attachment object at index %u", attachment_index);
                    continue;
                }

                // Confirm it is an object, not an array
                if (attachment_object.type != BSON_OBJECT_TYPE_OBJECT)
                {
                    BERROR("Expected object type of object for attachment. Skipping...");
                    continue;
                }

                // Attachment type
                const char* attachment_type_str = 0;
                if (!bson_object_property_value_get_string(&attachment_object, "type", &attachment_type_str))
                {
                    BERROR("Unable to determine attachment type. Skipping...");
                    continue;
                }
                scene_node_attachment_type attachment_type = scene_attachment_type_from_string(attachment_type_str);

                scene_node_attachment_config new_attachment = {0};
                new_attachment.type = attachment_type;

                // Deserialize the attachment
                switch (attachment_type)
                {
                case SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH:
                {
                    new_attachment.attachment_data = ballocate(sizeof(scene_node_attachment_static_mesh), MEMORY_TAG_SCENE);
                    if (!deserialize_scene_static_mesh_attachment(&attachment_object, new_attachment.attachment_data))
                    {
                        BERROR("Failed to deserialize attachment. Skipping...");
                        bfree(new_attachment.attachment_data, sizeof(scene_node_attachment_static_mesh), MEMORY_TAG_SCENE);
                        continue;
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_TERRAIN:
                {
                    new_attachment.attachment_data = ballocate(sizeof(scene_node_attachment_terrain), MEMORY_TAG_SCENE);
                    if (!deserialize_scene_terrain_attachment(&attachment_object, new_attachment.attachment_data))
                    {
                        BERROR("Failed to deserialize attachment. Skipping...");
                        bfree(new_attachment.attachment_data, sizeof(scene_node_attachment_terrain), MEMORY_TAG_SCENE);
                        continue;
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_SKYBOX:
                {
                    new_attachment.attachment_data = ballocate(sizeof(scene_node_attachment_skybox), MEMORY_TAG_SCENE);
                    if (!deserialize_scene_skybox_attachment(&attachment_object, new_attachment.attachment_data))
                    {
                        BERROR("Failed to deserialize attachment. Skipping...");
                        bfree(new_attachment.attachment_data, sizeof(scene_node_attachment_skybox), MEMORY_TAG_SCENE);
                        continue;
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT:
                {
                    new_attachment.attachment_data = ballocate(sizeof(scene_node_attachment_directional_light), MEMORY_TAG_SCENE);
                    if (!deserialize_scene_directional_light_attachment(&attachment_object, new_attachment.attachment_data))
                    {
                        BERROR("Failed to deserialize attachment. Skipping...");
                        bfree(new_attachment.attachment_data, sizeof(scene_node_attachment_directional_light), MEMORY_TAG_SCENE);
                        continue;
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT:
                {
                    new_attachment.attachment_data = ballocate(sizeof(scene_node_attachment_point_light), MEMORY_TAG_SCENE);
                    if (!deserialize_scene_point_light_attachment(&attachment_object, new_attachment.attachment_data))
                    {
                        BERROR("Failed to deserialize attachment. Skipping...");
                        bfree(new_attachment.attachment_data, sizeof(scene_node_attachment_point_light), MEMORY_TAG_SCENE);
                        continue;
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE:
                {
                    new_attachment.attachment_data = ballocate(sizeof(scene_node_attachment_water_plane), MEMORY_TAG_SCENE);
                    if (!deserialize_scene_water_plane_attachment(&attachment_object, new_attachment.attachment_data))
                    {
                        BERROR("Failed to deserialize attachment. Skipping...");
                        bfree(new_attachment.attachment_data, sizeof(scene_node_attachment_water_plane), MEMORY_TAG_SCENE);
                        continue;
                    }
                } break;
                default:
                case SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN:
                    BERROR("Attachment type is unknown. Skipping...");
                    continue;
                }

                // Push the attachment to the node's config
                if (!out_node_config->attachments)
                    out_node_config->attachments = darray_create(scene_node_attachment_config);
                darray_push(out_node_config->attachments, new_attachment);
            }
        }
    }

    // Process children, if any
    bson_object children_array = {0};
    if (bson_object_property_value_get_object(node_object, "children", &children_array))
    {
        // Make sure it is actually an array
        if (children_array.type == BSON_OBJECT_TYPE_ARRAY)
        {
            u32 child_count = 0;
            bson_array_element_count_get(&children_array, &child_count);

            // Each child
            for (u32 child_index = 0; child_index < child_count; ++child_index)
            {
                // Get the object
                bson_object child_object = {0};
                if (!bson_array_element_value_get_object(&children_array, child_index, &child_object))
                {
                    BERROR("Unable to get child object at index %u", child_index);
                    continue;
                }

                scene_node_config new_child = {0};

                // Deserialize the child node and push to the array if successful
                if (scene_node_config_deserialize_bson(&child_object, &new_child))
                {
                    // Push the child to the node's children
                    if (!out_node_config->children)
                        out_node_config->children = darray_create(scene_node_config);
                    darray_push(out_node_config->children, new_child);
                }
            }
        }
    }

    return true;
}

b8 scene_config_deserialize_bson(const bson_tree* source_tree, scene_config* scene)
{
    // Extract scene properties
    bson_object scene_properties_obj;
    if (!bson_object_property_value_get_object(&source_tree->root, "properties", &scene_properties_obj))
    {
        BERROR("Global scene properties missing. Using defaults");
        scene->name = "Untitled Scene";
        scene->description = "Default description";
    }
    else
    {
        // Extract name
        const char* name = 0;
        if (bson_object_property_value_get_string(&scene_properties_obj, "name", &name))
        {
            scene->name = string_duplicate(name);
        }
        else
        {
            // Use default
            scene->name = "Untitled Scene";
        }

        // Extract description
        const char* description = 0;
        if (bson_object_property_value_get_string(&scene_properties_obj, "description", &description))
        {
            scene->description = string_duplicate(description);
        }
        else
        {
            // Use default
            scene->description = "Default description.";
        }
    }

    // Process nodes
    scene->nodes = darray_create(scene_node_config);

    // Extract and process nodes
    bson_object scene_nodes_array;
    if (bson_object_property_value_get_object(&source_tree->root, "nodes", &scene_nodes_array))
    {
        // Only process if array
        if (scene_nodes_array.type != BSON_OBJECT_TYPE_ARRAY)
        {
            BERROR("Unexpected object named 'nodes' found. Expected array instead. Section will be skipped");
        }
        else
        {
            u32 node_count = 0;
            bson_array_element_count_get(&scene_nodes_array, &node_count);
            for (u32 node_index = 0; node_index < node_count; ++node_index)
            {
                // Setup a new node
                scene_node_config node_config = {0};

                // Get the node object
                bson_object node_object = {0};
                if (!bson_array_element_value_get_object(&scene_nodes_array, node_index, &node_object))
                {
                    BERROR("Failed to get node object at index %u", node_index);
                    continue;
                }

                // Deserialize the node and push to the array of root nodes if successful
                if (scene_node_config_deserialize_bson(&node_object, &node_config))
                    darray_push(scene->nodes, node_config);
            }
        }
    }

    return true;
}

static b8 scene_loader_load(struct resource_loader* self, const char* name, void* params, resource* out_resource)
{
    if (!self || !name || !out_resource)
        return false;

    char* format_str = "%s/%s/%s%s";
    char full_file_path[512];
    string_format_unsafe(full_file_path, format_str, resource_system_base_path(), self->type_path, name, ".bsn");

    file_handle f;
    if (!filesystem_open(full_file_path, FILE_MODE_READ, false, &f))
    {
        BERROR("scene_loader_load - unable to open scene file for reading: '%s'", full_file_path);
        return false;
    }

    out_resource->full_path = string_duplicate(full_file_path);

    u64 file_size = 0;
    if (!filesystem_size(&f, &file_size))
    {
        BERROR("Failed to check size of scene file");
        return false;
    }

    u64 bytes_read = 0;
    char* file_content = ballocate(file_size + 1, MEMORY_TAG_RESOURCE);
    if (!filesystem_read_all_text(&f, file_content, &bytes_read))
    {
        BERROR("Failed to read all text of scene file");
        return false;
    }

    filesystem_close(&f);

    // Verify that we read the whole file
    if (bytes_read != file_size)
        BWARN("File size/bytes read mismatch: %llu / %llu", file_size, bytes_read);

    // Parse the file.
    bson_tree source_tree = {0};
    if (!bson_tree_from_string(file_content, &source_tree))
    {
        BERROR("Failed to parse scene file. See logs for details");
        return false;
    }

    scene_config* resource_data = ballocate(sizeof(scene_config), MEMORY_TAG_RESOURCE);

    // Deserialize the scene
    if (!scene_config_deserialize_bson(&source_tree, resource_data))
    {
        BERROR("Failed to deserialize bson to scene config");
        bson_tree_cleanup(&source_tree);
        return false;
    }

    // Destroy the tree
    bson_tree_cleanup(&source_tree);

    out_resource->data = resource_data;
    out_resource->data_size = sizeof(scene_config);

    return true;
}

static void scene_loader_unload(struct resource_loader* self, resource* resource)
{
    scene_config* data = (scene_config*)resource->data;

    // TODO: properly destroy nodes, attachments, etc

    if (data->name)
        bfree(data->name, string_length(data->name) + 1, MEMORY_TAG_STRING);

    if (data->description)
        bfree(data->description, string_length(data->description) + 1, MEMORY_TAG_STRING);

    if (!resource_unload(self, resource, MEMORY_TAG_RESOURCE))
        BWARN("scene_loader_unload called with nullptr for self or resource");
}

resource_loader scene_resource_loader_create(void)
{
    resource_loader loader;
    loader.type = RESOURCE_TYPE_scene;
    loader.custom_type = 0;
    loader.load = scene_loader_load;
    loader.unload = scene_loader_unload;
    loader.type_path = "scenes";

    return loader;
}
