#include "scene.h"

#include "containers/darray.h"
#include "core/console.h"
#include "core/engine.h"
#include "core/frame_data.h"
#include "core_render_types.h"
#include "defines.h"
#include "graphs/hierarchy_graph.h"
#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "math/geometry_3d.h"
#include "math/bmath.h"
#include "math/math_types.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "platform/filesystem.h"
#include "renderer/renderer_types.h"
#include "resources/debug/debug_box3d.h"
#include "resources/debug/debug_line3d.h"
#include "resources/resource_types.h"
#include "resources/skybox.h"
#include "resources/terrain.h"
#include "resources/water_plane.h"
#include "strings/bname.h"
#include "strings/bstring.h"
#include "strings/bstring_id.h"
#include "systems/light_system.h"
#include "systems/material_system.h"
#include "systems/static_mesh_system.h"
#include "systems/xform_system.h"
#include "utils/bsort.h"

// TODO: remove this
#include <stdio.h> //sscanf

static void scene_actual_unload(scene* scene);
static void scene_node_metadata_ensure_allocated(scene* s, u64 handle_index);

static u32 global_scene_id = 0;

typedef struct scene_debug_data
{
    debug_box3d box;
    debug_line3d line;
} scene_debug_data;

/** @brief A private structure used to sort geometry by distance from the camera */
typedef struct geometry_distance
{
    // Geometry render data
    geometry_render_data g;
    // Distance from the camera
    f32 distance;
} geometry_distance;

static i32 geometry_render_data_compare(void* a, void* b)
{
    geometry_render_data* a_typed = a;
    geometry_render_data* b_typed = b;
    return a_typed->material.material.handle_index - b_typed->material.material.handle_index;
}

static i32 geometry_distance_compare(void* a, void* b)
{
    geometry_distance* a_typed = a;
    geometry_distance* b_typed = b;
    if (a_typed->distance > b_typed->distance)
        return 1;
    else if (a_typed->distance < b_typed->distance)
        return -1;

    return 0;
}

b8 scene_create(scene_config* config, scene_flags flags, scene* out_scene)
{
    if (!out_scene)
    {
        BERROR("scene_create(): A valid pointer to out_scene is required");
        return false;
    }

    bzero_memory(out_scene, sizeof(scene));

    out_scene->flags = flags;
    out_scene->enabled = false;
    out_scene->state = SCENE_STATE_UNINITIALIZED;
    global_scene_id++;
    out_scene->id = global_scene_id;

    // Internal "lists" of renderable objects
    out_scene->dir_lights = darray_create(directional_light);
    out_scene->point_lights = darray_create(point_light);
    out_scene->static_meshes = darray_create(static_mesh_instance);
    out_scene->terrains = darray_create(terrain);
    out_scene->skyboxes = darray_create(skybox);
    out_scene->water_planes = darray_create(water_plane);

    // Internal lists of attachments
    /* out_scene->attachments = darray_create(scene_attachment); */
    out_scene->mesh_attachments = darray_create(scene_attachment);
    out_scene->terrain_attachments = darray_create(scene_attachment);
    out_scene->skybox_attachments = darray_create(scene_attachment);
    out_scene->directional_light_attachments = darray_create(scene_attachment);
    out_scene->point_light_attachments = darray_create(scene_attachment);
    out_scene->water_plane_attachments = darray_create(scene_attachment);

    b8 is_readonly = ((out_scene->flags & SCENE_FLAG_READONLY) != 0);
    if (!is_readonly)
    {
        out_scene->mesh_metadata = darray_create(scene_static_mesh_metadata);
        out_scene->terrain_metadata = darray_create(scene_terrain_metadata);
        out_scene->skybox_metadata = darray_create(scene_skybox_metadata);
        out_scene->water_plane_metadata = darray_create(scene_water_plane_metadata);
    }

    if (!hierarchy_graph_create(&out_scene->hierarchy))
    {
        BERROR("Failed to create hierarchy graph");
        return false;
    }

    // TODO: Don't save off config beyond the scene being loaded. Destroy the config once loading is complete
    if (config)
    {
        out_scene->config = ballocate(sizeof(scene_config), MEMORY_TAG_SCENE);
        bcopy_memory(out_scene->config, config, sizeof(scene_config));

        out_scene->resource_name = string_duplicate(config->resource_name);
        out_scene->resource_full_path = string_duplicate(config->resource_full_path);
    }

    debug_grid_config grid_config = {0};
    grid_config.orientation = GRID_ORIENTATION_XZ;
    grid_config.segment_count_dim_0 = 100;
    grid_config.segment_count_dim_1 = 100;
    grid_config.segment_size = 1.0f;
    grid_config.name = bname_create("debug_grid");
    grid_config.use_third_axis = true;

    if (!debug_grid_create(&grid_config, &out_scene->grid))
        return false;

    return true;
}

void scene_destroy(scene* s)
{
    // TODO: actually destroy the thing and remove this
    /* scene_attachment_release(s, 0); */
}

void scene_node_initialize(scene* s, bhandle parent_handle, scene_node_config* node_config)
{
    if (node_config)
    {
        b8 is_readonly = ((s->flags & SCENE_FLAG_READONLY) != 0);

        // Obtain the xform if one is configured
        bhandle xform_handle;
        if (node_config->xform)
            xform_handle = xform_from_position_rotation_scale(node_config->xform->position, node_config->xform->rotation, node_config->xform->scale);
        else
            xform_handle = bhandle_invalid();

        // Add a node in the heirarchy
        bhandle node_handle = hierarchy_graph_child_add_with_xform(&s->hierarchy, parent_handle, xform_handle);

        if (!is_readonly)
        {
            scene_node_metadata_ensure_allocated(s, node_handle.handle_index);
            if (node_config->name)
            {
                scene_node_metadata* m = &s->node_metadata[node_handle.handle_index];
                m->id = node_handle.handle_index;
                m->name = string_duplicate(node_config->name);
            }
        }
        // TODO: Also do this for attachments where needed

        // Process attachment configs
        if (node_config->attachments)
        {
            u32 attachment_count = darray_length(node_config->attachments);
            for (u32 i = 0; i < attachment_count; ++i)
            {
                scene_node_attachment_config* attachment_config = &node_config->attachments[i];
                scene_node_attachment_type attachment_type = *((scene_node_attachment_type*)attachment_config);
                switch (attachment_type)
                {
                default:
                case SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN:
                    BERROR("An unknown attachment type was found in config. This attachment will be ignored");
                    continue;
                case SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH:
                {
                    scene_node_attachment_static_mesh* typed_attachment_config = attachment_config->attachment_data;

                    if (!typed_attachment_config->resource_name)
                    {
                        BWARN("Invalid mesh config, resource_name is required");
                        return;
                    }

                    static_mesh_instance new_static_mesh = {0};
                    if (!static_mesh_system_instance_acquire(engine_systems_get()->static_mesh_system, 0, bname_create(typed_attachment_config->resource_name), &new_static_mesh))
                    {
                        BERROR("Failed to create new static mesh in scene");
                        return;
                    }
                    else
                    {
                        // Find a free static mesh slot and take it, or push a new one
                        u32 resource_index = INVALID_ID;
                        u32 count = darray_length(s->static_meshes);
                        for (u32 i = 0; i < count; ++i)
                        {
                            if (s->static_meshes[i].instance_id == INVALID_ID_U64)
                            {
                                // Found a slot, use it
                                resource_index = i;
                                s->static_meshes[i] = new_static_mesh;
                                s->mesh_attachments[i].resource_handle = bhandle_create(resource_index);
                                s->mesh_attachments[i].hierarchy_node_handle = node_handle;
                                s->mesh_attachments[i].attachment_type = SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH;
                                // For "edit" mode, retain metadata
                                if (!is_readonly)
                                    s->mesh_metadata[i].resource_name = string_duplicate(typed_attachment_config->resource_name);
                                break;
                            }
                        }
                        if (resource_index == INVALID_ID)
                        {
                            darray_push(s->static_meshes, new_static_mesh);
                            resource_index = count;
                            scene_attachment mesh_attachment = {0};
                            mesh_attachment.resource_handle = bhandle_create(resource_index);
                            mesh_attachment.hierarchy_node_handle = node_handle;
                            mesh_attachment.attachment_type = SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH;
                            darray_push(s->mesh_attachments, mesh_attachment);
                            // For "edit" mode, retain metadata
                            if (!is_readonly)
                            {
                                scene_static_mesh_metadata new_mesh_metadata = {0};
                                new_mesh_metadata.resource_name = string_duplicate(typed_attachment_config->resource_name);
                                darray_push(s->mesh_metadata, new_mesh_metadata);
                            }
                        }
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_TERRAIN:
                {
                    scene_node_attachment_terrain* typed_attachment = attachment_config->attachment_data;

                    if (!typed_attachment->resource_name)
                    {
                        BWARN("Invalid terrain config, resource_name is required");
                        return;
                    }

                    terrain_config new_terrain_config = {0};
                    new_terrain_config.resource_name = string_duplicate(typed_attachment->resource_name);
                    new_terrain_config.name = string_duplicate(typed_attachment->name);
                    terrain new_terrain = {0};
                    if (!terrain_create(&new_terrain_config, &new_terrain))
                    {
                        BWARN("Failed to load terrain");
                        return;
                    }

                    // Destroy the config
                    string_free(new_terrain_config.resource_name);
                    string_free(new_terrain_config.name);

                    if (!terrain_initialize(&new_terrain))
                    {
                        BERROR("Failed to initialize terrain");
                        return;
                    }
                    else
                    {
                        // Find a free static terrain slot and take it, or push a new one
                        u32 index = INVALID_ID;
                        u32 count = darray_length(s->terrains);
                        for (u32 i = 0; i < count; ++i)
                        {
                            if (s->terrains[i].state == TERRAIN_STATE_UNDEFINED)
                            {
                                // Found a slot, use it
                                index = i;
                                s->terrains[i] = new_terrain;
                                s->terrain_attachments[i].resource_handle = bhandle_create(index);
                                s->terrain_attachments[i].hierarchy_node_handle = node_handle;
                                s->terrain_attachments[i].attachment_type = SCENE_NODE_ATTACHMENT_TYPE_TERRAIN;
                                // For "edit" mode, retain metadata
                                if (!is_readonly)
                                {
                                    s->terrain_metadata[i].resource_name = string_duplicate(typed_attachment->resource_name);
                                    s->terrain_metadata[i].name = string_duplicate(typed_attachment->name);
                                }
                                break;
                            }
                        }
                        if (index == INVALID_ID)
                        {
                            darray_push(s->terrains, new_terrain);
                            index = count;
                            scene_attachment terrain_attachment = {0};
                            terrain_attachment.resource_handle = bhandle_create(index);
                            terrain_attachment.hierarchy_node_handle = node_handle;
                            terrain_attachment.attachment_type = SCENE_NODE_ATTACHMENT_TYPE_TERRAIN;
                            darray_push(s->terrain_attachments, terrain_attachment);
                            // For "edit" mode, retain metadata
                            if (!is_readonly)
                            {
                                scene_terrain_metadata new_terrain_metadata = {0};
                                new_terrain_metadata.resource_name = string_duplicate(typed_attachment->resource_name);
                                new_terrain_metadata.name = string_duplicate(typed_attachment->name);
                                darray_push(s->terrain_metadata, new_terrain_metadata);
                            }
                        }
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_SKYBOX:
                {
                    scene_node_attachment_skybox* typed_attachment = attachment_config->attachment_data;

                    // Create a skybox config and use it to create the skybox
                    skybox_config sb_config = {0};
                    sb_config.cubemap_name = string_duplicate(typed_attachment->cubemap_name);
                    skybox sb;
                    if (!skybox_create(sb_config, &sb))
                        BWARN("Failed to create skybox");

                    // Destroy the skybox config
                    string_free((char*)sb_config.cubemap_name);
                    sb_config.cubemap_name = 0;

                    // Initialize the skybox
                    if (!skybox_initialize(&sb))
                    {
                        BERROR("Failed to initialize skybox. See logs for details");
                    }
                    else
                    {
                        // Find a free skybox slot and take it, or push a new one
                        u32 index = INVALID_ID;
                        u32 skybox_count = darray_length(s->skyboxes);
                        for (u32 i = 0; i < skybox_count; ++i)
                        {
                            if (s->skyboxes[i].state == SKYBOX_STATE_UNDEFINED)
                            {
                                // Found a slot, use it
                                index = i;
                                s->skyboxes[i] = sb;
                                s->skybox_attachments[i].resource_handle = bhandle_create(index);
                                s->skybox_attachments[i].hierarchy_node_handle = node_handle;
                                s->skybox_attachments[i].attachment_type = SCENE_NODE_ATTACHMENT_TYPE_SKYBOX;
                                // For "edit" mode, retain metadata
                                if (!is_readonly)
                                    s->skybox_metadata[i].cubemap_name = string_duplicate(typed_attachment->cubemap_name);
                                break;
                            }
                        }
                        if (index == INVALID_ID)
                        {
                            darray_push(s->skyboxes, sb);
                            index = skybox_count;
                            scene_attachment skybox_attachment = {0};
                            skybox_attachment.resource_handle = bhandle_create(index);
                            skybox_attachment.hierarchy_node_handle = node_handle;
                            skybox_attachment.attachment_type = SCENE_NODE_ATTACHMENT_TYPE_SKYBOX;
                            darray_push(s->skybox_attachments, skybox_attachment);
                            // For "edit" mode, retain metadata
                            if (!is_readonly)
                            {
                                scene_skybox_metadata new_skybox_metadata = {0};
                                new_skybox_metadata.cubemap_name = string_duplicate(typed_attachment->cubemap_name);
                                darray_push(s->skybox_metadata, new_skybox_metadata);
                            }
                        }
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT:
                {
                    scene_node_attachment_directional_light* typed_attachment = attachment_config->attachment_data;

                    directional_light new_dir_light = {0};
                    // TODO: name?
                    /* new_dir_light.name = string_duplicate(typed_attachment.name); */
                    new_dir_light.data.color = typed_attachment->color;
                    new_dir_light.data.direction = typed_attachment->direction;
                    new_dir_light.data.shadow_distance = typed_attachment->shadow_distance;
                    new_dir_light.data.shadow_fade_distance = typed_attachment->shadow_fade_distance;
                    new_dir_light.data.shadow_split_mult = typed_attachment->shadow_split_mult;
                    new_dir_light.generation = 0;

                    // Add debug data and initialize it
                    new_dir_light.debug_data = ballocate(sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                    scene_debug_data* debug = new_dir_light.debug_data;

                    // Generate the line points based on the light direction
                    // The first point will always be at the scene's origin
                    vec3 point_0 = vec3_zero();
                    vec3 point_1 = vec3_mul_scalar(vec3_normalized(vec3_from_vec4(new_dir_light.data.direction)), -1.0f);

                    if (!debug_line3d_create(point_0, point_1, bhandle_invalid(), &debug->line))
                        BERROR("Failed to create debug line for directional light");
                    if (!debug_line3d_initialize(&debug->line))
                    {
                        BERROR("Failed to create debug line for directional light");
                    }
                    else
                    {
                        // Find a free skybox slot and take it, or push a new one
                        u32 index = INVALID_ID;
                        u32 directional_light_count = darray_length(s->dir_lights);
                        for (u32 i = 0; i < directional_light_count; ++i)
                        {
                            if (s->dir_lights[i].generation == INVALID_ID)
                            {
                                // Found a slot, use it
                                index = i;
                                s->dir_lights[i] = new_dir_light;
                                s->directional_light_attachments[i].resource_handle = bhandle_create(index);
                                s->directional_light_attachments[i].hierarchy_node_handle = node_handle;
                                s->directional_light_attachments[i].attachment_type = SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT;
                                break;
                            }
                        }
                        if (index == INVALID_ID)
                        {
                            darray_push(s->dir_lights, new_dir_light);
                            index = directional_light_count;
                            scene_attachment directional_light_attachment = {0};
                            directional_light_attachment.resource_handle = bhandle_create(index);
                            directional_light_attachment.hierarchy_node_handle = node_handle;
                            directional_light_attachment.attachment_type = SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT;
                            darray_push(s->directional_light_attachments, directional_light_attachment);
                        }
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT:
                {
                    scene_node_attachment_point_light* typed_attachment = attachment_config->attachment_data;

                    point_light new_light = {0};
                    // TODO: name?
                    /* new_light.name = string_duplicate(typed_attachment->name); */
                    new_light.data.color = typed_attachment->color;
                    new_light.data.constant_f = typed_attachment->constant_f;
                    new_light.data.linear = typed_attachment->linear;
                    // Set the base position, not the world position, which will be calculated on update
                    new_light.position = typed_attachment->position;
                    new_light.data.quadratic = typed_attachment->quadratic;

                    // Add debug data and initialize it
                    new_light.debug_data = ballocate(sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                    scene_debug_data* debug = new_light.debug_data;

                    if (!debug_box3d_create((vec3){0.2f, 0.2f, 0.2f}, bhandle_invalid(), &debug->box))
                    {
                        BERROR("Failed to create debug box for directional light");
                    }
                    else
                    {
                        xform_position_set(debug->box.xform, vec3_from_vec4(new_light.data.position));
                    }

                    if (!debug_box3d_initialize(&debug->box))
                    {
                        BERROR("Failed to create debug box for point light");
                    }
                    else
                    {
                        // Find a free skybox slot and take it, or push a new one
                        u32 index = INVALID_ID;
                        u32 point_light_count = darray_length(s->point_lights);
                        for (u32 i = 0; i < point_light_count; ++i)
                        {
                            if (s->point_lights[i].generation == INVALID_ID)
                            {
                                // Found a slot, use it
                                index = i;
                                s->point_lights[i] = new_light;
                                s->point_light_attachments[i].resource_handle = bhandle_create(index);
                                s->point_light_attachments[i].hierarchy_node_handle = node_handle;
                                s->point_light_attachments[i].attachment_type = SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT;
                                break;
                            }
                        }
                        if (index == INVALID_ID)
                        {
                            darray_push(s->point_lights, new_light);
                            index = point_light_count;
                            scene_attachment point_light_attachment = {0};
                            point_light_attachment.resource_handle = bhandle_create(index);
                            point_light_attachment.hierarchy_node_handle = node_handle;
                            point_light_attachment.attachment_type = SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT;
                            darray_push(s->point_light_attachments, point_light_attachment);
                        }
                    }
                } break;
                case SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE:
                {
                    scene_node_attachment_water_plane* typed_attachment = attachment_config->attachment_data;

                    // Create a water_plane config and use it to create the skybox
                    // water_plane_config sb_config = {0};
                    // sb_config.cubemap_name = string_duplicate(typed_attachment->cubemap_name);
                    water_plane wp;
                    if (!water_plane_create(&wp))
                        BWARN("Failed to create water plane");

                    // Destroy the water plane config
                    // string_free((char*)sb_config.cubemap_name);
                    // sb_config.cubemap_name = 0;

                    // Initialize the water plane
                    if (!water_plane_initialize(&wp))
                    {
                        BERROR("Failed to initialize water plane. See logs for details");
                    }
                    else
                    {
                        // Find a free skybox slot and take it, or push a new one
                        u32 index = INVALID_ID;
                        u32 water_plane_count = darray_length(s->water_planes);
                        for (u32 i = 0; i < water_plane_count; ++i)
                        {
                            // TODO: water plane states
                            // if (s->water_planes[i].state == WATER_PLANE_STATE_UNDEFINED) {
                            if (false)
                            {
                                // Found a slot, use it
                                index = i;
                                s->water_planes[i] = wp;
                                s->water_plane_attachments[i].resource_handle = bhandle_create(index);
                                s->water_plane_attachments[i].hierarchy_node_handle = node_handle;
                                s->water_plane_attachments[i].attachment_type = SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE;
                                // For "edit" mode, retain metadata
                                if (!is_readonly)
                                    s->water_plane_metadata[i].reserved = typed_attachment->reserved;
                                break;
                            }
                        }
                        if (index == INVALID_ID)
                        {
                            darray_push(s->water_planes, wp);
                            index = water_plane_count;
                            scene_attachment water_plane_attachment = {0};
                            water_plane_attachment.resource_handle = bhandle_create(index);
                            water_plane_attachment.hierarchy_node_handle = node_handle;
                            water_plane_attachment.attachment_type = SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE;
                            darray_push(s->water_plane_attachments, water_plane_attachment);
                            // For "edit" mode, retain metadata
                            if (!is_readonly)
                            {
                                scene_water_plane_metadata new_water_plane_metadata = {0};
                                new_water_plane_metadata.reserved = typed_attachment->reserved;
                                darray_push(s->water_plane_metadata, new_water_plane_metadata);
                            }
                        }
                    }
                } break;
                }
            }
        }

        // Process children
        if (node_config->children)
        {
            u32 child_count = darray_length(node_config->children);
            for (u32 i = 0; i < child_count; ++i)
                scene_node_initialize(s, node_handle, &node_config->children[i]);
        }
    }
}

b8 scene_initialize(scene* scene)
{
    if (!scene)
    {
        BERROR("scene_initialize requires a valid pointer to a scene");
        return false;
    }

    // Process configuration and setup hierarchy
    if (scene->config)
    {
        scene_config* config = scene->config;
        if (scene->config->name)
            scene->name = string_duplicate(scene->config->name);
        if (scene->config->description)
            scene->description = string_duplicate(scene->config->description);

        // Process root nodes
        if (config->nodes)
        {
            u32 node_count = darray_length(config->nodes);
            // An invalid handle means there is no parent, which is true for root nodes
            bhandle invalid_handle = bhandle_invalid();
            for (u32 i = 0; i < node_count; ++i)
                scene_node_initialize(scene, invalid_handle, &config->nodes[i]);
        }

        // TODO: Convert grid to use the new node/attachment configs/logic
        if (!debug_grid_initialize(&scene->grid))
            return false;
    }

    // Update the state to show the scene is initialized
    scene->state = SCENE_STATE_INITIALIZED;

    return true;
}

b8 scene_load(scene* scene)
{
    if (!scene)
        return false;

    // Update the state to show the scene is currently loading
    scene->state = SCENE_STATE_LOADING;

    // Register with the console
    console_object_register("scene", scene, CONSOLE_OBJECT_TYPE_STRUCT);
    console_object_add_property("scene", "id", &scene->id, CONSOLE_OBJECT_TYPE_UINT32);

    // Load skyboxes
    if (scene->skyboxes)
    {
        u32 skybox_count = darray_length(scene->skyboxes);
        for (u32 i = 0; i < skybox_count; ++i)
        {
            if (!skybox_load(&scene->skyboxes[i]))
                BERROR("Failed to load skybox. See logs for details");
        }
    }

    // Load static meshes
    if (scene->static_meshes)
    {
        u32 mesh_count = darray_length(scene->static_meshes);
        for (u32 i = 0; i < mesh_count; ++i)
        {
            // TODO: is this needed anymore
            /* if (!mesh_load(&scene->static_meshes[i]))
                BERROR("Mesh failed to load");
            */
        }
    }

    // Load terrains
    if (scene->terrains)
    {
        u32 terrain_count = darray_length(scene->terrains);
        for (u32 i = 0; i < terrain_count; ++i)
        {
            if (!terrain_load(&scene->terrains[i]))
                BERROR("Terrain failed to load");
        }
    }

    // Load water planes
    if (scene->water_planes)
    {
        u32 water_plane_count = darray_length(scene->water_planes);
        for (u32 i = 0; i < water_plane_count; ++i)
        {
            if (!water_plane_load(&scene->water_planes[i]))
                BERROR("Failed to load water plane. See logs for details");
        }
    }

    // Debug grid
    if (!debug_grid_load(&scene->grid))
        return false;

    if (scene->dir_lights)
    {
        u32 directional_light_count = darray_length(scene->dir_lights);
        for (u32 i = 0; i < directional_light_count; ++i)
        {
            if (!light_system_directional_add(&scene->dir_lights[i]))
            {
                BWARN("Failed to add directional light to lighting system");
            }
            else
            {
                if (scene->dir_lights[i].debug_data)
                {
                    scene_debug_data* debug = scene->dir_lights[i].debug_data;
                    if (!debug_line3d_load(&debug->line))
                    {
                        BERROR("debug line failed to load");
                        bfree(scene->dir_lights[i].debug_data, sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                        scene->dir_lights[i].debug_data = 0;
                    }
                }
            }
        }
    }

    if (scene->point_lights)
    {
        u32 point_light_count = darray_length(scene->point_lights);
        for (u32 i = 0; i < point_light_count; ++i)
        {
            if (!light_system_point_add(&scene->point_lights[i]))
            {
                BWARN("Failed to add point light to lighting system");
            }
            else
            {
                // Load debug data if it was setup
                scene_debug_data* debug = (scene_debug_data*)scene->point_lights[i].debug_data;
                if (!debug_box3d_load(&debug->box))
                {
                    BERROR("debug box failed to load");
                    bfree(scene->point_lights[i].debug_data, sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                    scene->point_lights[i].debug_data = 0;
                }
            }
        }
    }

    // Update the state to show the scene is fully loaded
    scene->state = SCENE_STATE_LOADED;

    return true;
}

b8 scene_unload(scene* scene, b8 immediate)
{
    if (!scene)
        return false;
    
    // Always immediately set the state to unloading
    scene->state = SCENE_STATE_UNLOADING;

    // If immediate, trigger the unload right away. Otherwise it will happen on the next frame
    if (immediate)
    {
        scene_actual_unload(scene);
        return true;
    }

    return true;
}

b8 scene_update(scene* scene, const struct frame_data* p_frame_data)
{
    if (!scene)
        return false;

    if (scene->state == SCENE_STATE_UNLOADING)
    {
        scene_actual_unload(scene);
        return true;
    }

    if (scene->state >= SCENE_STATE_LOADED)
    {
        hierarchy_graph_update(&scene->hierarchy);

        if (scene->dir_lights)
        {
            u32 directional_light_count = darray_length(scene->dir_lights);
            for (u32 i = 0; i < directional_light_count; ++i)
            {
                // TODO: Only update directional light if changed
                if (scene->dir_lights[i].generation != INVALID_ID && scene->dir_lights[i].debug_data)
                {
                    scene_debug_data* debug = scene->dir_lights[i].debug_data;
                    if (debug->line.geometry.generation != INVALID_ID_U16)
                    {
                        // Update color. NOTE: doing this every frame might be expensive if we have to reload the geometry all the time.
                        debug_line3d_color_set(&debug->line, scene->dir_lights[i].data.color);
                    }
                }
            }
        }

        // Update point light debug boxes
        if (scene->point_lights)
        {
            u32 point_light_count = darray_length(scene->point_lights);
            for (u32 i = 0; i < point_light_count; ++i)
            {
                // Update the point light's data position (world position) to take into account the owning node's transform
                scene_attachment* point_light_attachment = &scene->point_light_attachments[i];
                bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, point_light_attachment->hierarchy_node_handle);

                mat4 world;
                if (!bhandle_is_invalid(xform_handle))
                {
                    world = xform_world_get(xform_handle);
                }
                else
                {
                    // TODO: traverse tree to try and find a ancestor node with a transform
                    world = mat4_identity();
                }

                // Calculate world position for the point light
                /* scene->point_lights[i].data.position = vec4_mul_mat4(scene->point_lights[i].position, world); */
                // TODO: the below method works, the above does not
                vec3 pos = vec3_from_vec4(scene->point_lights[i].position);
                scene->point_lights[i].data.position = vec4_from_vec3(vec3_transform(pos, 1.0f, world), 1.0f);

                // Debug box info update
                if (scene->point_lights[i].debug_data)
                {
                    // TODO: Only update point light if changed
                    scene_debug_data* debug = (scene_debug_data*)scene->point_lights[i].debug_data;
                    if (debug->box.geometry.generation != INVALID_ID_U16)
                    {
                        // Update transform
                        xform_position_set(debug->box.xform, vec3_from_vec4(scene->point_lights[i].data.position));

                        // Update color. NOTE: doing this every frame might be expensive if we have to reload the geometry all the time
                        debug_box3d_color_set(&debug->box, scene->point_lights[i].data.color);
                    }
                }
            }
        }

        // Check meshes to see if they have debug data. If not, add it here and init/load it.
        // Doing this here because mesh loading is multi-threaded, and may not yet be available even though the object is present in the scene
        u32 mesh_count = darray_length(scene->static_meshes);
        for (u32 i = 0; i < mesh_count; ++i)
        {
            // TODO: debug data - refactor this or find another way to handle it
            /* static_mesh_instance* m = &scene->static_meshes[i];
            if (m->generation == INVALID_ID_U8)
                continue;

            if (!m->debug_data)
            {
                m->debug_data = ballocate(sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                scene_debug_data* debug = m->debug_data;

                if (!debug_box3d_create((vec3){0.2f, 0.2f, 0.2f}, bhandle_invalid(), &debug->box))
                {
                    BERROR("Failed to create debug box for mesh '%s'", m->name);
                }
                else
                {
                    // Lookup the attachment to get the xform handle to set as the parent
                    scene_attachment* attachment = &scene->mesh_attachments[i];
                    bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
                    // Since debug objects aren't actually added to the hierarchy or as attachments, need to manually update
                    // the xform here, using the node's world xform as the parent
                    xform_calculate_local(debug->box.xform);
                    mat4 local = xform_local_get(debug->box.xform);
                    mat4 parent_world = xform_world_get(xform_handle);
                    mat4 model = mat4_mul(local, parent_world);
                    xform_world_set(debug->box.xform, model);

                    if (!debug_box3d_initialize(&debug->box))
                    {
                        BERROR("debug box failed to initialize");
                        bfree(m->debug_data, sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                        m->debug_data = 0;
                        continue;
                    }

                    if (!debug_box3d_load(&debug->box))
                    {
                        BERROR("debug box failed to load");
                        bfree(m->debug_data, sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                        m->debug_data = 0;
                    }

                    // Update the extents
                    debug_box3d_color_set(&debug->box, (vec4){0.0f, 1.0f, 0.0f, 1.0f});
                    debug_box3d_extents_set(&debug->box, m->extents);
                }
            } */
        }
    }

    return true;
}

void scene_render_frame_prepare(scene* scene, const struct frame_data* p_frame_data)
{
    if (!scene)
        return;

    if (scene->state >= SCENE_STATE_LOADED)
    {
        if (scene->dir_lights)
        {
            u32 directional_light_count = darray_length(scene->dir_lights);
            for (u32 i = 0; i < directional_light_count; ++i)
            {
                if (scene->dir_lights[i].generation != INVALID_ID && scene->dir_lights[i].debug_data)
                {
                    scene_debug_data* debug = scene->dir_lights[i].debug_data;
                    debug_line3d_render_frame_prepare(&debug->line, p_frame_data);
                }
            }
        }

        // Update point light debug boxes
        if (scene->point_lights)
        {
            u32 point_light_count = darray_length(scene->point_lights);
            for (u32 i = 0; i < point_light_count; ++i)
            {
                if (scene->point_lights[i].debug_data)
                {
                    scene_debug_data* debug = (scene_debug_data*)scene->point_lights[i].debug_data;

                    // Lookup the attachment to get the xform handle to set as the parent
                    scene_attachment* attachment = &scene->point_light_attachments[i];
                    bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
                    // Since debug objects aren't actually added to the hierarchy or as attachments, need to manually update the xform here, using the node's world xform as the parent
                    xform_calculate_local(debug->box.xform);
                    mat4 local = xform_local_get(debug->box.xform);
                    mat4 parent_world = xform_world_get(xform_handle);
                    mat4 model = mat4_mul(local, parent_world);
                    xform_world_set(debug->box.xform, model);

                    debug_box3d_render_frame_prepare(&debug->box, p_frame_data);
                }
            }
        }

        // Check meshes to see if they have debug data
        if (scene->static_meshes)
        {
            u32 mesh_count = darray_length(scene->static_meshes);
            for (u32 i = 0; i < mesh_count; ++i)
            {
                // TODO: debug data - refactor or find another way to do it
                /* static_mesh_instance* m = &scene->static_meshes[i];
                if (m->generation == INVALID_ID_U8)
                    continue;

                if (m->debug_data)
                {
                    scene_debug_data* debug = m->debug_data;

                    // Lookup the attachment to get the xform handle to set as the parent
                    scene_attachment* attachment = &scene->mesh_attachments[i];
                    bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
                    // Since debug objects aren't actually added to the hierarchy or as attachments, need to manually update the xform here, using the node's world xform as the parent
                    xform_calculate_local(debug->box.xform);
                    mat4 local = xform_local_get(debug->box.xform);
                    mat4 parent_world = xform_world_get(xform_handle);
                    mat4 model = mat4_mul(local, parent_world);
                    xform_world_set(debug->box.xform, model);

                    debug_box3d_render_frame_prepare(&debug->box, p_frame_data);
                } */
            }
        }
    }
}

void scene_update_lod_from_view_position(scene* scene, const frame_data* p_frame_data, vec3 view_position, f32 near_clip, f32 far_clip)
{
    if (!scene)
        return;

    if (scene->state >= SCENE_STATE_LOADED)
    {
        // Update terrain chunk LODs
        u32 terrain_count = darray_length(scene->terrains);
        for (u32 i = 0; i < terrain_count; ++i)
        {
            terrain* t = &scene->terrains[i];

            // Perform a lookup into the attachments array to get the hierarchy node
            scene_attachment* attachment = &scene->terrain_attachments[i];
            bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
            mat4 model = xform_world_get(xform_handle);

            // Calculate LOD splits based on clip range
            f32 range = far_clip - near_clip;

            // The first split distance is always 0
            f32* splits = p_frame_data->allocator.allocate(sizeof(f32) * (t->lod_count + 1));
            splits[0] = 0.0f;
            for (u32 l = 0; l < t->lod_count; ++l)
            {
                f32 pct = (l + 1) / (f32)t->lod_count;
                // Just do linear splits for now
                splits[l + 1] = (near_clip + range) * pct;
            }

            // Calculate chunk LODs based on distance from camera position
            for (u32 c = 0; c < t->chunk_count; c++)
            {
                terrain_chunk* chunk = &t->chunks[c];

                // Translate/scale the center
                vec3 g_center = vec3_mul_mat4(chunk->center, model);

                // Check the distance of the chunk
                f32 dist_to_chunk = vec3_distance(view_position, g_center);
                u8 lod = INVALID_ID_U8;
                for (u8 l = 0; l < t->lod_count; ++l)
                {
                    // If between this and the next split, this is the LOD to use
                    if (dist_to_chunk >= splits[l] && dist_to_chunk <= splits[l + 1])
                    {
                        lod = l;
                        break;
                    }
                }
                // Cover the case of chunks outside the view frustum
                if (lod == INVALID_ID_U8)
                    lod = t->lod_count - 1;

                chunk->current_lod = lod;
            }
        }
    }
}

b8 scene_raycast(scene* scene, const struct ray* r, struct raycast_result* out_result)
{
    if (!scene || !r || !out_result || scene->state < SCENE_STATE_LOADED)
        return false;

    // Only create if needed
    out_result->hits = 0;

    // Iterate meshes in the scene
    // TODO: This needs to be optimized
    u32 mesh_count = darray_length(scene->static_meshes);
    for (u32 i = 0; i < mesh_count; ++i)
    {
        static_mesh_instance* m = &scene->static_meshes[i];
        // Perform a lookup into the attachments array to get the hierarchy node
        scene_attachment* attachment = &scene->mesh_attachments[i];
        bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
        mat4 model = xform_world_get(xform_handle);
        f32 dist;
        // FIXME: This just selects the first geometry's extents. Need to add extents to the whole thing based on all submeshes
        if (raycast_oriented_extents(m->mesh_resource->submeshes[0].geometry.extents, model, r, &dist))
        {
            // Hit
            if (!out_result->hits)
                out_result->hits = darray_create(raycast_hit);

            raycast_hit hit = {0};
            hit.distance = dist;
            hit.type = RAYCAST_HIT_TYPE_OBB;
            hit.position = vec3_add(r->origin, vec3_mul_scalar(r->direction, hit.distance));

            hit.xform_handle = xform_handle;
            hit.node_handle = attachment->hierarchy_node_handle;

            // Get parent xform handle if one exists
            hit.xform_parent_handle = hierarchy_graph_parent_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
            // TODO: Indicate selection node attachment type

            darray_push(out_result->hits, hit);
        }
    }

    // Sort the results based on distance
    if (out_result->hits)
    {
        b8 swapped;
        u32 length = darray_length(out_result->hits);
        for (u32 i = 0; i < length - 1; ++i)
        {
            swapped = false;
            for (u32 j = 0; j < length - 1; ++j)
            {
                if (out_result->hits[j].distance > out_result->hits[j + 1].distance)
                {
                    BSWAP(raycast_hit, out_result->hits[j], out_result->hits[j + 1]);
                    swapped = true;
                }
            }

            // If no 2 elements were swapped, then sort is complete
            if (!swapped)
                break;
        }
    }
    return out_result->hits != 0;
}

b8 scene_debug_render_data_query(scene* scene, u32* data_count, geometry_render_data** debug_geometries)
{
    if (!scene || !data_count)
        return false;

    *data_count = 0;

    // TODO: Check if grid exists
    // TODO: flag for toggling grid on and off
    if (false)
    {
        if (debug_geometries)
        {
            geometry_render_data data = {0};
            data.model = mat4_identity();

            bgeometry* g = &scene->grid.geometry;
            data.material.material = bhandle_invalid(); // debug geometries don't need a material
            data.material.instance = bhandle_invalid(); // debug geometries don't need a material
            data.vertex_count = g->vertex_count;
            data.vertex_buffer_offset = g->vertex_buffer_offset;
            data.index_count = g->index_count;
            data.index_buffer_offset = g->index_buffer_offset;
            data.unique_id = INVALID_ID;

            (*debug_geometries)[(*data_count)] = data;
        }
        (*data_count)++;
    }

    // Directional light
    {
        if (debug_geometries && scene->dir_lights)
        {
            u32 directional_light_count = darray_length(scene->dir_lights);
            for (u32 i = 0; i < directional_light_count; ++i)
            {
                if (scene->dir_lights[i].debug_data)
                {
                    scene_debug_data* debug = scene->dir_lights[i].debug_data;

                    // Debug line 3d
                    geometry_render_data data = {0};
                    data.model = xform_world_get(debug->line.xform);
                    bgeometry* g = &debug->line.geometry;
                    data.material.material = bhandle_invalid(); // debug geometries don't need a material
                    data.material.instance = bhandle_invalid(); // debug geometries don't need a material
                    data.vertex_count = g->vertex_count;
                    data.vertex_buffer_offset = g->vertex_buffer_offset;
                    data.index_count = g->index_count;
                    data.index_buffer_offset = g->index_buffer_offset;
                    data.unique_id = debug->line.id.uniqueid;

                    (*debug_geometries)[(*data_count)] = data;
                }
                (*data_count)++;
            }
        }
    }

    // Point lights
    {
        if (scene->point_lights)
        {
            u32 point_light_count = darray_length(scene->point_lights);
            for (u32 i = 0; i < point_light_count; ++i)
            {
                if (scene->point_lights[i].debug_data)
                {
                    if (debug_geometries)
                    {
                        scene_debug_data* debug = (scene_debug_data*)scene->point_lights[i].debug_data;

                        // Debug box 3d
                        geometry_render_data data = {0};
                        data.model = xform_world_get(debug->box.xform);
                        bgeometry* g = &debug->box.geometry;
                        data.material.material = bhandle_invalid(); // debug geometries don't need a material
                        data.material.instance = bhandle_invalid(); // debug geometries don't need a material
                        data.vertex_count = g->vertex_count;
                        data.vertex_buffer_offset = g->vertex_buffer_offset;
                        data.index_count = g->index_count;
                        data.index_buffer_offset = g->index_buffer_offset;
                        data.unique_id = debug->box.id.uniqueid;

                        (*debug_geometries)[(*data_count)] = data;
                    }
                    (*data_count)++;
                }
            }
        }
    }

    // Mesh debug shapes
    {
        if (scene->static_meshes)
        {
            u32 mesh_count = darray_length(scene->static_meshes);
            for (u32 i = 0; i < mesh_count; ++i)
            {
                // TODO: debug data - refactor or find another way to do it
                /* if (scene->static_meshes[i].debug_data)
                {
                    if (debug_geometries)
                    {
                        scene_debug_data* debug = (scene_debug_data*)scene->static_meshes[i].debug_data;

                        // Debug box 3d
                        geometry_render_data data = {0};
                        data.model = xform_world_get(debug->box.xform);
                        geometry* g = &debug->box.geo;
                        data.material = g->material;
                        data.vertex_count = g->vertex_count;
                        data.vertex_buffer_offset = g->vertex_buffer_offset;
                        data.index_count = g->index_count;
                        data.index_buffer_offset = g->index_buffer_offset;
                        data.unique_id = debug->box.id.uniqueid;

                        (*debug_geometries)[(*data_count)] = data;
                    }
                    (*data_count)++;
                } */
            }
        }
    }

    return true;
}

b8 scene_mesh_render_data_query_from_line(const scene* scene, vec3 direction, vec3 center, f32 radius, frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries)
{
    if (!scene)
        return false;

    geometry_distance* transparent_geometries = darray_create_with_allocator(geometry_distance, &p_frame_data->allocator);

    u32 mesh_count = darray_length(scene->static_meshes);
    for (u32 i = 0; i < mesh_count; ++i)
    {
        static_mesh_instance* m = &scene->static_meshes[i];
        scene_attachment* attachment = &scene->mesh_attachments[i];
        bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
        mat4 model = xform_world_get(xform_handle);

        // TODO: Cache this somewhere instead of calculating all the time
        f32 determinant = mat4_determinant(model);
        b8 winding_inverted = determinant < 0;

        for (u32 j = 0; j < m->mesh_resource->submesh_count; ++j)
        {
            static_mesh_submesh* submesh = &m->mesh_resource->submeshes[j];
            bgeometry* g = &submesh->geometry;

            // TODO: cache this somewhere...

            // Translate/scale the extents
            vec3 extents_min = vec3_mul_mat4(g->extents.min, model);
            vec3 extents_max = vec3_mul_mat4(g->extents.max, model);
            // Translate/scale the center
            vec3 transformed_center = vec3_mul_mat4(g->center, model);
            // Find the one furthest from the center
            f32 mesh_radius = BMAX(vec3_distance(extents_min, transformed_center), vec3_distance(extents_max, transformed_center));

            f32 dist_to_line = vec3_distance_to_line(transformed_center, center, direction);

            // Is within distance, so include it
            if ((dist_to_line - mesh_radius) <= radius)
            {
                // Add it to the list to be rendered
                geometry_render_data data = {0};
                data.model = model;
                data.material = m->material_instances[j];
                data.vertex_count = g->vertex_count;
                data.vertex_buffer_offset = g->vertex_buffer_offset;
                data.index_count = g->index_count;
                data.index_buffer_offset = g->index_buffer_offset;
                data.unique_id = 0; // m->id.uniqueid; FIXME: Need this for pixel selection
                data.winding_inverted = winding_inverted;

                // Check if transparent. If so, put into a separate, temp array to be
                // sorted by distance from the camera. Otherwise, put into the out_geometries array directly
                b8 has_transparency = material_flag_get(engine_systems_get()->material_system, m->material_instances[j].material, BMATERIAL_FLAG_HAS_TRANSPARENCY_BIT);

                if (has_transparency)
                {
                    // NOTE: This isn't perfect for translucent meshes that intersect, but is enough for our purposes now
                    vec3 geometry_center = vec3_transform(g->center, 1.0f, model);
                    f32 distance = vec3_distance(geometry_center, center);

                    geometry_distance gdist;
                    gdist.distance = babs(distance);
                    gdist.g = data;
                    darray_push(transparent_geometries, gdist);
                }
                else
                {
                    darray_push(*out_geometries, data);
                }
                p_frame_data->drawn_mesh_count++;
            }
        }
    }

    // Sort opaque geometries by material
    bquick_sort(sizeof(geometry_render_data), *out_geometries, 0, darray_length(*out_geometries) - 1, geometry_render_data_compare);

    // Sort transparent geometries, then add them to the ext_data->geometries array
    u32 transparent_geometry_count = darray_length(transparent_geometries);
    bquick_sort(sizeof(geometry_distance), transparent_geometries, 0, transparent_geometry_count - 1, geometry_distance_compare);
    for (u32 i = 0; i < transparent_geometry_count; ++i)
        darray_push(*out_geometries, transparent_geometries[i].g);

    *out_count = darray_length(*out_geometries);

    return true;
}

b8 scene_terrain_render_data_query_from_line(const scene* scene, vec3 direction, vec3 center, f32 radius, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries)
{
    if (!scene)
        return false;

    u32 terrain_count = darray_length(scene->terrains);
    for (u32 i = 0; i < terrain_count; ++i)
    {
        terrain* t = &scene->terrains[i];
        scene_attachment* attachment = &scene->terrain_attachments[i];
        bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
        mat4 model = xform_world_get(xform_handle);

        // TODO: Cache this somewhere instead of calculating all the time
        f32 determinant = mat4_determinant(model);
        b8 winding_inverted = determinant < 0;

        // Check each chunk to see if it is in view
        for (u32 c = 0; c < t->chunk_count; ++c)
        {
            terrain_chunk* chunk = &t->chunks[c];

            if (chunk->generation != INVALID_ID_U16)
            {
                // TODO: cache this somewhere
                // Translate/scale the extents
                vec3 extents_min = vec3_mul_mat4(chunk->extents.min, model);
                vec3 extents_max = vec3_mul_mat4(chunk->extents.max, model);
                // Translate/scale the center
                vec3 transformed_center = vec3_mul_mat4(chunk->center, model);
                // Find the one furthest from the center
                f32 mesh_radius = BMAX(vec3_distance(extents_min, transformed_center), vec3_distance(extents_max, transformed_center));

                f32 dist_to_line = vec3_distance_to_line(transformed_center, center, direction);

                // Is within distance, so include it
                if ((dist_to_line - mesh_radius) <= radius)
                {
                    // Add it to the list to be rendered
                    geometry_render_data data = {0};
                    data.model = model;
                    data.material = chunk->material;
                    data.vertex_count = chunk->total_vertex_count;
                    data.vertex_buffer_offset = chunk->vertex_buffer_offset;

                    // Use the indices for the current LOD
                    data.index_count = chunk->lods[chunk->current_lod].total_index_count;
                    data.index_buffer_offset = chunk->lods[chunk->current_lod].index_buffer_offset;
                    data.index_element_size = sizeof(u32);
                    data.unique_id = t->id.uniqueid;
                    data.winding_inverted = winding_inverted;

                    darray_push(*out_geometries, data);
                }
            }
        }
    }

    *out_count = darray_length(*out_geometries);

    return true;
}

b8 scene_mesh_render_data_query(const scene* scene, const frustum* f, vec3 center, frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries)
{
    if (!scene)
        return false;

    geometry_distance* transparent_geometries = darray_create_with_allocator(geometry_distance, &p_frame_data->allocator);

    // Iterate all meshes in the scene
    u32 mesh_count = darray_length(scene->static_meshes);
    for (u32 resource_index = 0; resource_index < mesh_count; ++resource_index)
    {
        static_mesh_instance* m = &scene->static_meshes[resource_index];
        // Attachment lookup - by resource index
        scene_attachment* attachment = &scene->mesh_attachments[resource_index];
        bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
        mat4 model = xform_world_get(xform_handle);

        // TODO: Cache this somewhere instead of calculating all the time
        f32 determinant = mat4_determinant(model);
        b8 winding_inverted = determinant < 0;

        for (u32 j = 0; j < m->mesh_resource->submesh_count; ++j)
        {
            static_mesh_submesh* submesh = &m->mesh_resource->submeshes[j];
            bgeometry* g = &submesh->geometry;

            // AABB calculation
            {
                // Translate/scale the extents
                // vec3 extents_min = vec3_mul_mat4(g->extents.min, model);
                vec3 extents_max = vec3_mul_mat4(g->extents.max, model);

                // Translate/scale the center
                vec3 g_center = vec3_mul_mat4(g->center, model);
                vec3 half_extents = {
                    babs(extents_max.x - g_center.x),
                    babs(extents_max.y - g_center.y),
                    babs(extents_max.z - g_center.z),
                };

                if (!f || frustum_intersects_aabb(f, &g_center, &half_extents))
                {
                    // Add it to the list to be rendered
                    geometry_render_data data = {0};
                    data.model = model;
                    data.material = m->material_instances[j];
                    data.vertex_count = g->vertex_count;
                    data.vertex_buffer_offset = g->vertex_buffer_offset;
                    data.index_count = g->index_count;
                    data.index_buffer_offset = g->index_buffer_offset;
                    data.unique_id = 0; // m->id.uniqueid; FIXME: needed for per-pixel selection
                    data.winding_inverted = winding_inverted;

                    // Check if transparent. If so, put into a separate, temp array to be
                    // sorted by distance from the camera. Otherwise, put into the out_geometries array directly
                    b8 has_transparency = material_flag_get(engine_systems_get()->material_system, m->material_instances[j].material, BMATERIAL_FLAG_HAS_TRANSPARENCY_BIT);
                    if (has_transparency)
                    {
                        // NOTE: This isn't perfect for translucent meshes that intersect, but is enough for our purposes now
                        f32 distance = vec3_distance(g_center, center);

                        geometry_distance gdist;
                        gdist.distance = babs(distance);
                        gdist.g = data;
                        darray_push(transparent_geometries, gdist);
                    }
                    else
                    {
                        darray_push(*out_geometries, data);
                    }
                    p_frame_data->drawn_mesh_count++;
                }
            }
        }
    }

    // Sort opaque geometries by material
    bquick_sort(sizeof(geometry_render_data), *out_geometries, 0, darray_length(*out_geometries) - 1, geometry_render_data_compare);

    // Sort transparent geometries, then add them to the ext_data->geometries array
    u32 transparent_geometry_count = darray_length(transparent_geometries);
    bquick_sort(sizeof(geometry_distance), transparent_geometries, 0, transparent_geometry_count - 1, geometry_distance_compare);
    for (u32 i = 0; i < transparent_geometry_count; ++i)
        darray_push(*out_geometries, transparent_geometries[i].g);

    *out_count = darray_length(*out_geometries);

    return true;
}

b8 scene_terrain_render_data_query(const scene* scene, const frustum* f, vec3 center, frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_terrain_geometries)
{
    if (!scene)
        return false;

    u32 terrain_count = darray_length(scene->terrains);
    for (u32 i = 0; i < terrain_count; ++i)
    {
        terrain* t = &scene->terrains[i];
        scene_attachment* attachment = &scene->terrain_attachments[i];
        bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
        mat4 model = xform_world_get(xform_handle);

        // TODO: Cache this somewhere instead of calculating all the time
        f32 determinant = mat4_determinant(model);
        b8 winding_inverted = determinant < 0;

        // Check each chunk to see if it is in view
        for (u32 c = 0; c < t->chunk_count; c++)
        {
            terrain_chunk* chunk = &t->chunks[c];

            if (chunk->generation != INVALID_ID_U16)
            {
                // AABB calculation
                vec3 g_center, half_extents;

                if (f)
                {
                    // TODO: cache this somewhere
                    //
                    // Translate/scale the extents
                    vec3 extents_max = vec3_mul_mat4(chunk->extents.max, model);

                    // Translate/scale the center
                    g_center = vec3_mul_mat4(chunk->center, model);
                    half_extents = (vec3){
                        babs(extents_max.x - g_center.x),
                        babs(extents_max.y - g_center.y),
                        babs(extents_max.z - g_center.z),
                    };
                }

                if (!f || frustum_intersects_aabb(f, &g_center, &half_extents))
                {
                    geometry_render_data data = {0};
                    data.model = model;
                    data.material = chunk->material;
                    data.vertex_count = chunk->total_vertex_count;
                    data.vertex_buffer_offset = chunk->vertex_buffer_offset;
                    data.vertex_element_size = sizeof(terrain_vertex);

                    // Use the indices for the current LOD
                    data.index_count = chunk->lods[chunk->current_lod].total_index_count;
                    data.index_buffer_offset = chunk->lods[chunk->current_lod].index_buffer_offset;
                    data.index_element_size = sizeof(u32);
                    data.unique_id = t->id.uniqueid;
                    data.winding_inverted = winding_inverted;

                    darray_push(*out_terrain_geometries, data);
                }
            }
        }
    }

    *out_count = darray_length(*out_terrain_geometries);

    return true;
}

/**
 * @brief Gets a count and optionally an array of water planes from the given scene.
 *
 * @param scene A constant pointer to the scene.
 * @param f A constant pointer to the frustum to use for culling.
 * @param center The center view point.
 * @param p_frame_data A pointer to the current frame's data.
 * @param out_count A pointer to hold the count.
 * @param out_water_planes A pointer to an array of pointers to water planes. Pass 0 if just obtaining the count.
 * @return True on success; otherwise false.
 */
b8 scene_water_plane_query(const scene* scene, const frustum* f, vec3 center, frame_data* p_frame_data, u32* out_count, water_plane*** out_water_planes)
{
    if (!scene)
        return false;
    *out_count = 0;

    u32 count = 0;
    u32 water_plane_count = darray_length(scene->water_planes);
    for (u32 i = 0; i < water_plane_count; ++i)
    {
        if (out_water_planes)
        {
            // scene_attachment* attachment = &scene->mesh_attachments[i];
            // bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
            // mat4 model = xform_world_get(xform_handle);

            water_plane* wp = &scene->water_planes[i];
            scene_attachment* attachment = &scene->water_plane_attachments[i];
            bhandle xform_handle = hierarchy_graph_xform_handle_get(&scene->hierarchy, attachment->hierarchy_node_handle);
            // FIXME: World should work here, but for some reason isn't being updated...
            wp->model = xform_local_get(xform_handle);
            darray_push(*out_water_planes, wp);
        }
        count++;
    }

    *out_count = count;

    return true;
}

static void scene_actual_unload(scene* s)
{
    u32 skybox_count = darray_length(s->skyboxes);
    for (u32 i = 0; i < skybox_count; ++i)
    {
        if (!skybox_unload(&s->skyboxes[i]))
            BERROR("Failed to unload skybox");
        skybox_destroy(&s->skyboxes[i]);
        s->skyboxes[i].state = SKYBOX_STATE_UNDEFINED;
    }

    u32 mesh_count = darray_length(s->static_meshes);
    for (u32 i = 0; i < mesh_count; ++i)
    {
        if (s->static_meshes[i].instance_id != INVALID_ID_U64)
        {
            // Unload any debug data
            // TODO: debug data
            /* if (s->static_meshes[i].debug_data)
            {
                scene_debug_data* debug = s->static_meshes[i].debug_data;

                debug_box3d_unload(&debug->box);
                debug_box3d_destroy(&debug->box);

                bfree(s->static_meshes[i].debug_data, sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
                s->static_meshes[i].debug_data = 0;
            } */

            static_mesh_system_instance_release(engine_systems_get()->static_mesh_system, &s->static_meshes[i]);
            s->static_meshes->instance_id = INVALID_ID_U64;
        }
    }

    u32 terrain_count = darray_length(s->terrains);
    for (u32 i = 0; i < terrain_count; ++i)
    {
        if (!terrain_unload(&s->terrains[i]))
            BERROR("Failed to unload terrain");
        terrain_destroy(&s->terrains[i]);
    }

    // Debug grid
    if (!debug_grid_unload(&s->grid))
        BWARN("Debug grid unload failed");

    u32 directional_light_count = darray_length(s->dir_lights);
    for (u32 i = 0; i < directional_light_count; ++i)
    {
        if (!light_system_directional_remove(&s->dir_lights[i]))
            BERROR("Failed to unload/remove directional light");

        s->dir_lights[i].generation = INVALID_ID;

        if (s->dir_lights[i].debug_data)
        {
            scene_debug_data* debug = (scene_debug_data*)s->dir_lights[i].debug_data;
            // Unload directional light line data
            debug_line3d_unload(&debug->line);
            debug_line3d_destroy(&debug->line);
            bfree(s->dir_lights[i].debug_data, sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
            s->dir_lights[i].debug_data = 0;
        }
    }

    u32 p_light_count = darray_length(s->point_lights);
    for (u32 i = 0; i < p_light_count; ++i)
    {
        if (!light_system_point_remove(&s->point_lights[i]))
            BWARN("Failed to remove point light from light system");

        // Destroy debug data if it exists
        if (s->point_lights[i].debug_data)
        {
            scene_debug_data* debug = (scene_debug_data*)s->point_lights[i].debug_data;
            debug_box3d_unload(&debug->box);
            debug_box3d_destroy(&debug->box);
            bfree(s->point_lights[i].debug_data, sizeof(scene_debug_data), MEMORY_TAG_RESOURCE);
            s->point_lights[i].debug_data = 0;
        }
    }

    u32 water_plane_count = darray_length(s->water_planes);
    for (u32 i = 0; i < water_plane_count; ++i)
    {
        if (!water_plane_unload(&s->water_planes[i]))
            BERROR("Failed to unload water plane");
        water_plane_destroy(&s->water_planes[i]);
        // s->water_planes[i].state = WATER_PLANE_STATE_UNDEFINED;
    }

    // Destroy the hierarchy graph
    hierarchy_graph_destroy(&s->hierarchy);

    // Update the state to show the scene is initialized
    s->state = SCENE_STATE_UNLOADED;

    // Also destroy the scene
    if (s->skyboxes)
        darray_destroy(s->skyboxes);
    if (s->dir_lights)
        darray_destroy(s->dir_lights);
    if (s->point_lights)
        darray_destroy(s->point_lights);
    if (s->static_meshes)
        darray_destroy(s->static_meshes);
    if (s->terrains)
        darray_destroy(s->terrains);
    if (s->water_planes)
        darray_destroy(s->water_planes);

    bzero_memory(s, sizeof(scene));
}

static b8 scene_serialize_node(const scene* s, const hierarchy_graph_view* view, const hierarchy_graph_view_node* view_node, bson_property* node)
{
    if (!s || !view || !view_node)
        return false;

    // Serialize top-level node metadata, etc.
    scene_node_metadata* node_meta = &s->node_metadata[view_node->node_handle.handle_index];

    // Node name
    bson_object_value_add_string(&node->value.o, "name", node_meta->name);

    // xform is optional, so make sure there is a valid handle to one before serializing
    if (!bhandle_is_invalid(view_node->xform_handle))
        bson_object_value_add_string(&node->value.o, "xform", xform_to_string(view_node->xform_handle));

    // Attachments
    bson_property attachments_prop = {0};
    attachments_prop.type = BSON_PROPERTY_TYPE_ARRAY;
    attachments_prop.name = bstring_id_create("attachments");
    attachments_prop.value.o.type = BSON_OBJECT_TYPE_ARRAY;
    attachments_prop.value.o.properties = darray_create(bson_property);

    // Look through each attachment type and see if the hierarchy_node_handle matches the node
    // handle of the current node being serialized. If it does, use the resource_handle to
    // index into the resource and/or metadata arrays to obtain needed data.
    // TODO: A relational view that allows for easy lookups of attachments for a particular node

    // Meshes
    u32 mesh_count = darray_length(s->mesh_attachments);
    for (u32 m = 0; m < mesh_count; ++m)
    {
        if (s->mesh_attachments[m].hierarchy_node_handle.handle_index == view_node->node_handle.handle_index)
        {
            // Create the object array entry
            bson_property attachment = bson_object_property_create(0);

            // Add properties to it
            bson_object_value_add_string(&attachment.value.o, "type", "static_mesh");
            bson_object_value_add_string(&attachment.value.o, "resource_name", s->mesh_metadata[m].resource_name);

            // Push it into the attachments array
            darray_push(attachments_prop.value.o.properties, attachment);
        }
    }

    // skyboxes
    u32 skybox_count = darray_length(s->skybox_attachments);
    for (u32 m = 0; m < skybox_count; ++m)
    {
        if (s->skybox_attachments[m].hierarchy_node_handle.handle_index == view_node->node_handle.handle_index)
        {
            // Found one!

            // Create the object array entry
            bson_property attachment = bson_object_property_create(0);

            // Add properties to it
            bson_object_value_add_string(&attachment.value.o, "type", "skybox");
            bson_object_value_add_string(&attachment.value.o, "cubemap_name", s->skybox_metadata[m].cubemap_name);

            // Push it into the attachments array
            darray_push(attachments_prop.value.o.properties, attachment);
        }
    }

    // Terrains
    u32 terrain_count = darray_length(s->terrain_attachments);
    for (u32 m = 0; m < terrain_count; ++m)
    {
        if (s->terrain_attachments[m].hierarchy_node_handle.handle_index == view_node->node_handle.handle_index)
        {
            // Create the object array entry
            bson_property attachment = bson_object_property_create(0);

            // Add properties to it
            bson_object_value_add_string(&attachment.value.o, "type", "terrain");
            bson_object_value_add_string(&attachment.value.o, "name", s->terrain_metadata[m].name);
            bson_object_value_add_string(&attachment.value.o, "resource_name", s->terrain_metadata[m].resource_name);

            // Push it into the attachments array
            darray_push(attachments_prop.value.o.properties, attachment);
        }
    }

    // Point lights
    u32 point_light_count = darray_length(s->point_light_attachments);
    for (u32 m = 0; m < point_light_count; ++m)
    {
        if (s->point_light_attachments[m].hierarchy_node_handle.handle_index == view_node->node_handle.handle_index)
        {
            // Create the object array entry
            bson_property attachment = bson_object_property_create(0);

            // Add properties to it
            bson_object_value_add_string(&attachment.value.o, "type", "point_light");
            bson_object_value_add_string(&attachment.value.o, "color", vec4_to_string(s->point_lights[m].data.color));

            // NOTE: use the base light position, not the .data.positon since .data.position is the recalculated world position based on inherited transforms form parent node(s)
            bson_object_value_add_string(&attachment.value.o, "position", vec4_to_string(s->point_lights[m].position));
            bson_object_value_add_float(&attachment.value.o, "constant_f", s->point_lights[m].data.constant_f);
            bson_object_value_add_float(&attachment.value.o, "linear", s->point_lights[m].data.linear);
            bson_object_value_add_float(&attachment.value.o, "quadratic", s->point_lights[m].data.quadratic);

            // Push it into the attachments array
            darray_push(attachments_prop.value.o.properties, attachment);
        }
    }

    // Directional lights
    u32 directional_light_count = darray_length(s->directional_light_attachments);
    for (u32 m = 0; m < directional_light_count; ++m)
    {
        if (s->directional_light_attachments[m].hierarchy_node_handle.handle_index == view_node->node_handle.handle_index)
        {
            // Create the object array entry
            bson_property attachment = bson_object_property_create(0);

            // Add properties to it
            bson_object_value_add_string(&attachment.value.o, "type", "directional_light");
            bson_object_value_add_string(&attachment.value.o, "color", vec4_to_string(s->dir_lights[m].data.color));
            bson_object_value_add_string(&attachment.value.o, "direction", vec4_to_string(s->dir_lights[m].data.direction));
            bson_object_value_add_float(&attachment.value.o, "shadow_distance", s->dir_lights[m].data.shadow_distance);
            bson_object_value_add_float(&attachment.value.o, "shadow_fade_distance", s->dir_lights[m].data.shadow_fade_distance);
            bson_object_value_add_float(&attachment.value.o, "shadow_split_mult", s->dir_lights[m].data.shadow_split_mult);

            // Push it into the attachments array
            darray_push(attachments_prop.value.o.properties, attachment);
        }
    }

    // Water planes
    u32 water_plane_count = darray_length(s->water_plane_attachments);
    for (u32 m = 0; m < water_plane_count; ++m)
    {
        if (s->water_plane_attachments[m].hierarchy_node_handle.handle_index == view_node->node_handle.handle_index)
        {
            // Found one!

            // Create the object array entry
            bson_property attachment = bson_object_property_create(0);

            // Add properties to it
            bson_object_value_add_string(&attachment.value.o, "type", "water_plane");
            bson_object_value_add_int(&attachment.value.o, "reserved", s->water_plane_metadata[m].reserved);

            // Push it into the attachments array
            darray_push(attachments_prop.value.o.properties, attachment);
        }
    }

    darray_push(node->value.o.properties, attachments_prop);

    // Serialize children
    if (view_node->children)
    {
        u32 child_count = darray_length(view_node->children);

        if (child_count > 0)
        {
            // Only create the children property if the node actually has them
            bson_property children_prop = {0};
            children_prop.type = BSON_PROPERTY_TYPE_ARRAY;
            children_prop.name = bstring_id_create("children");
            children_prop.value.o.type = BSON_OBJECT_TYPE_ARRAY;
            children_prop.value.o.properties = darray_create(bson_property);
            for (u32 i = 0; i < child_count; ++i)
            {
                u32 index = view_node->children[i];
                hierarchy_graph_view_node* view_node = &view->nodes[index];

                bson_property child_node = {0};
                child_node.type = BSON_PROPERTY_TYPE_OBJECT;
                child_node.name = 0; // No name for array elements
                child_node.value.o.type = BSON_OBJECT_TYPE_OBJECT;
                child_node.value.o.properties = darray_create(bson_property);

                if (!scene_serialize_node(s, view, view_node, &child_node))
                {
                    BERROR("Failed to serialize node, see logs for details");
                    return false;
                }

                // Add the node to the array
                darray_push(children_prop.value.o.properties, child_node);
            }

            darray_push(node->value.o.properties, children_prop);
        }
    }

    return true;
}

b8 string_to_scene_xform_config(const char* str, scene_xform_config* out_xform)
{
    if (!str || !out_xform)
        return false;

    bzero_memory(out_xform, sizeof(scene_xform_config));
    f32 values[7] = {0};

    i32 count = sscanf(
        str,
        "%f %f %f %f %f %f %f %f %f %f",
        &out_xform->position.x, &out_xform->position.y, &out_xform->position.z,
        &values[0], &values[1], &values[2], &values[3], &values[4], &values[5], &values[6]);

    if (count == 10)
    {
        // Treat as quat, load directly
        out_xform->rotation.x = values[0];
        out_xform->rotation.y = values[1];
        out_xform->rotation.z = values[2];
        out_xform->rotation.w = values[3];

        // Set scale
        out_xform->scale.x = values[4];
        out_xform->scale.y = values[5];
        out_xform->scale.z = values[6];
    }
    else if (count == 9)
    {
        quat x_rot = quat_from_axis_angle((vec3){1.0f, 0, 0}, deg_to_rad(values[0]), true);
        quat y_rot = quat_from_axis_angle((vec3){0, 1.0f, 0}, deg_to_rad(values[1]), true);
        quat z_rot = quat_from_axis_angle((vec3){0, 0, 1.0f}, deg_to_rad(values[2]), true);
        out_xform->rotation = quat_mul(x_rot, quat_mul(y_rot, z_rot));

        // Set scale
        out_xform->scale.x = values[3];
        out_xform->scale.y = values[4];
        out_xform->scale.z = values[5];
    }
    else
    {
        BWARN("Format error: invalid xform provided. Identity transform will be used");
        out_xform->position = vec3_zero();
        out_xform->rotation = quat_identity();
        out_xform->scale = vec3_one();
        return false;
    }

    return true;
}

b8 scene_save(scene* s)
{
    if (!s)
    {
        BERROR("scene_save requires a valid pointer to a scene");
        return false;
    }

    if (s->flags & SCENE_FLAG_READONLY)
    {
        BERROR("Cannot save scene that is marked as read-only");
        return false;
    }

    bson_tree tree = {0};
    // The root of the tree
    tree.root.type = BSON_OBJECT_TYPE_OBJECT;
    tree.root.properties = darray_create(bson_property);

    // Properties property
    bson_property properties = bson_object_property_create("properties");

    bson_object_value_add_string(&properties.value.o, "name", s->name);
    bson_object_value_add_string(&properties.value.o, "description", s->description);

    darray_push(tree.root.properties, properties);

    // nodes
    bson_property nodes_prop = bson_array_property_create("nodes");

    hierarchy_graph_view* view = &s->hierarchy.view;
    if (view->root_indices)
    {
        u32 root_count = darray_length(view->root_indices);
        for (u32 i = 0; i < root_count; ++i)
        {
            u32 index = view->root_indices[i];
            hierarchy_graph_view_node* view_node = &view->nodes[index];

            bson_property node = {0};
            node.type = BSON_PROPERTY_TYPE_OBJECT;
            node.name = 0; // No name for array elements
            node.value.o.type = BSON_OBJECT_TYPE_OBJECT;
            node.value.o.properties = darray_create(bson_property);

            if (!scene_serialize_node(s, view, view_node, &node))
            {
                BERROR("Failed to serialize node, see logs for details");
                return false;
            }

            // Add the node to the array
            darray_push(nodes_prop.value.o.properties, node);
        }
    }

    // Push the nodes array object into the root properties
    darray_push(tree.root.properties, nodes_prop);

    // Write the contents of the tree to a string
    const char* file_content = bson_tree_to_string(&tree);
    BTRACE("File content: \n%s", file_content);

    // Cleanup the tree
    bson_tree_cleanup(&tree);

    // Write to file

    // TODO: Validate resource path and/or retrieve based on resource type and resource_name
    BINFO("Writing scene '%s' to file '%s'", s->name, s->resource_full_path);
    b8 result = true;
    file_handle f;
    if (!filesystem_open(s->resource_full_path, FILE_MODE_WRITE, false, &f))
    {
        BERROR("scene_save - unable to open scene file for writing: '%s'", s->resource_full_path);
        result = false;
        goto scene_save_file_cleanup;
    }

    u32 content_length = string_length(file_content);
    u64 bytes_written = 0;
    result = filesystem_write(&f, sizeof(char) * content_length, file_content, &bytes_written);
    if (!result)
        BERROR("Failed to write scene file");

scene_save_file_cleanup:
    string_free((char*)file_content);

    // Close the file
    filesystem_close(&f);
    return result;
}

static void scene_node_metadata_ensure_allocated(scene* s, u64 handle_index)
{
    if (s && handle_index != INVALID_ID_U64)
    {
        u64 new_count = handle_index + 1;
        if (s->node_metadata_count < new_count)
        {
            scene_node_metadata* new_array = ballocate(sizeof(scene_node_metadata) * new_count, MEMORY_TAG_SCENE);
            if (s->node_metadata)
            {
                bcopy_memory(new_array, s->node_metadata, sizeof(scene_node_metadata) * s->node_metadata_count);
                bfree(s->node_metadata, sizeof(scene_node_metadata) * s->node_metadata_count, MEMORY_TAG_SCENE);
            }
            s->node_metadata = new_array;

            // Invalidate all new entries
            for (u32 i = s->node_metadata_count; i < new_count; ++i)
                s->node_metadata[i].id = INVALID_ID;

            s->node_metadata_count = new_count;
        }
    }
    else
    {
        BWARN("scene_node_metadata_ensure_allocated requires a valid pointer to a scene, and a valid handle index");
    }
}
