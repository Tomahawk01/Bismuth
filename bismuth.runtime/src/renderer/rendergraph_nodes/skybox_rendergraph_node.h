#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "renderer/viewport.h"

struct rendergraph_node;
struct rendergraph_node_config;
struct frame_data;
struct rendergraph;

struct skybox;

BAPI b8 skybox_rendergraph_node_create(struct rendergraph* graph, struct rendergraph_node* self, const struct rendergraph_node_config* config);
BAPI b8 skybox_rendergraph_node_initialize(struct rendergraph_node* self);
BAPI b8 skybox_rendergraph_node_load_resources(struct rendergraph_node* self);
BAPI b8 skybox_rendergraph_node_execute(struct rendergraph_node* self, struct frame_data* p_frame_data);
BAPI void skybox_rendergraph_node_destroy(struct rendergraph_node* self);

BAPI void skybox_rendergraph_node_set_skybox(struct rendergraph_node* self, struct skybox* sb);
BAPI void skybox_rendergraph_node_set_viewport_and_matrices(struct rendergraph_node* self, viewport vp, mat4 view, mat4 projection);

b8 skybox_rendergraph_node_register_factory(void);
