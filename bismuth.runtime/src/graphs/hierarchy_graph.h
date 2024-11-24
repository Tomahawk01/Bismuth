#pragma once

#include "identifiers/bhandle.h"
#include "math/math_types.h"

struct frame_data;

typedef struct hierarchy_graph_view_node
{
    bhandle node_handle;
    bhandle xform_handle;

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
    bhandle* node_handles;
    u32* parent_indices;
    u8* levels;
    b8* dirty_flags;

    bhandle* xform_handles;

    hierarchy_graph_view view;
} hierarchy_graph;

BAPI b8 hierarchy_graph_create(hierarchy_graph* out_graph);
BAPI void hierarchy_graph_destroy(hierarchy_graph* graph);

BAPI void hierarchy_graph_update(hierarchy_graph* graph);

BAPI bhandle hierarchy_graph_xform_handle_get(const hierarchy_graph* graph, bhandle node_handle);
BAPI bhandle hierarchy_graph_parent_handle_get(const hierarchy_graph* graph, bhandle node_handle);
BAPI bhandle hierarchy_graph_parent_xform_handle_get(const hierarchy_graph* graph, bhandle node_handle);

BAPI bhandle hierarchy_graph_root_add(hierarchy_graph* graph);
BAPI bhandle hierarchy_graph_root_add_with_xform(hierarchy_graph* graph, bhandle xform_handle);
BAPI bhandle hierarchy_graph_child_add(hierarchy_graph* graph, bhandle parent_node_handle);
BAPI bhandle hierarchy_graph_child_add_with_xform(hierarchy_graph* graph, bhandle parent_node_handle, bhandle xform_handle);

BAPI void hierarchy_graph_node_remove(hierarchy_graph* graph, bhandle* node_handle, b8 release_xform);
BAPI quat hierarchy_graph_world_rotation_get(const hierarchy_graph* graph, bhandle node_handle);
BAPI vec3 hierarchy_graph_world_scale_get(const hierarchy_graph* graph, bhandle node_handle);
