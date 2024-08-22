#pragma once

#include "defines.h"
#include "renderer/viewport.h"

struct rendergraph;
struct rendergraph_node;
struct rendergraph_node_config;
struct frame_data;
struct editor_gizmo;

struct geometry_render_data;

b8 editor_gizmo_rendergraph_node_create(struct rendergraph* graph, struct rendergraph_node* self, const struct rendergraph_node_config* config);
b8 editor_gizmo_rendergraph_node_initialize(struct rendergraph_node* self);
b8 editor_gizmo_rendergraph_node_load_resources(struct rendergraph_node* self);
b8 editor_gizmo_rendergraph_node_execute(struct rendergraph_node* self, struct frame_data* p_frame_data);
void editor_gizmo_rendergraph_node_destroy(struct rendergraph_node* self);

BAPI b8 editor_gizmo_rendergraph_node_viewport_set(struct rendergraph_node* self, viewport v);
BAPI b8 editor_gizmo_rendergraph_node_view_projection_set(struct rendergraph_node* self, mat4 view_matrix, vec3 view_pos, mat4 projection_matrix);
BAPI b8 editor_gizmo_rendergraph_node_enabled_set(struct rendergraph_node* self, b8 enabled);
BAPI b8 editor_gizmo_rendergraph_node_gizmo_set(struct rendergraph_node* self, struct editor_gizmo* gizmo);

b8 editor_gizmo_rendergraph_node_register_factory(void);
