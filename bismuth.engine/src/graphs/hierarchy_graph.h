#ifndef _XFORM_GRAPH_H_
#define _XFORM_GRAPH_H_

#include "core/bhandle.h"
#include "math/math_types.h"

struct frame_data;

typedef struct hierarchy_graph_view_node
{
    b_handle node_handle;
    b_handle xform_handle;

    u32 parent_index;

    u32* children;
} hierarchy_graph_view_node;

typedef struct hierarchy_graph_view
{
    hierarchy_graph_view_node* nodes;
    u32* root_indices;
} hierarchy_graph_view;

typedef struct hierarchy_graph
{
    u32 nodes_allocated;
    b_handle* node_handles;
    u32* parent_indices;
    u8* levels;
    b8* dirty_flags;

    b_handle* xform_handles;

    hierarchy_graph_view view;
} hierarchy_graph;

BAPI b8 hierarchy_graph_create(hierarchy_graph* out_graph);
BAPI void hierarchy_graph_destroy(hierarchy_graph* graph);

BAPI void hierarchy_graph_update_tree_view_node(hierarchy_graph* graph, u32 node_index);

BAPI void hierarchy_graph_update(hierarchy_graph* graph, const struct frame_data* p_frame_data);

BAPI b_handle hierarchy_graph_root_add(hierarchy_graph* graph);
BAPI b_handle hierarchy_graph_root_add_with_xform(hierarchy_graph* graph, b_handle xform_handle);
BAPI b_handle hierarchy_graph_child_add(hierarchy_graph* graph, b_handle parent_node_handle);
BAPI b_handle hierarchy_graph_child_add_with_xform(hierarchy_graph* graph, b_handle parent_node_handle, b_handle xform_handle);
BAPI void hierarchy_graph_node_remove(hierarchy_graph* graph, b_handle* node_handle, b8 release_transform);

BAPI quat hierarchy_graph_world_rotation_get(hierarchy_graph* graph, b_handle node_handle);

BAPI vec3 hierarchy_graph_world_position_get(hierarchy_graph* graph, b_handle node_handle);

BAPI vec3 hierarchy_graph_world_scale_get(hierarchy_graph* graph, b_handle node_handle);

#endif
