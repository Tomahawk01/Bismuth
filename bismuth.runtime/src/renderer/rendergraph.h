#ifndef _RENDERGRAPH_H_
#define _RENDERGRAPH_H_

#include "defines.h"
#include "renderer/renderer_types.h"
#include "resources/resource_types.h"

#define RG_CHECK(expr)                            \
    if (!expr)                                    \
    {                                             \
        BERROR("Failed to execute: '%s'", #expr); \
        return false;                             \
    }

struct texture;
struct rendergraph_system_state;

typedef enum rendergraph_resource_type
{
    RENDERGRAPH_RESOURCE_TYPE_UNDEFINED,
    RENDERGRAPH_RESOURCE_TYPE_TEXTURE,
    RENDERGRAPH_RESOURCE_TYPE_NUMBER,
    RENDERGRAPH_RESOURCE_TYPE_MAX
} rendergraph_resource_type;

typedef struct rendergraph_source
{
    char* name;
    b8 is_bound;
    rendergraph_resource_type type;

    union
    {
        struct texture* t;
        u64 u64;
    } value;
} rendergraph_source;

typedef struct rendergraph_sink
{
    const char* name;
    const char* configured_source_name;
    rendergraph_resource_type type;
    rendergraph_source* bound_source;
} rendergraph_sink;

struct rendergraph;

typedef struct rendergraph_node
{
    u32 index;
    const char* name;

    // The graph owning this node
    struct rendergraph* graph;

    u32 source_count;
    rendergraph_source* sources;
    
    u32 sink_count;
    rendergraph_sink* sinks;

    void* internal_data;

    b8 (*initialize)(struct rendergraph_node* self);
    b8 (*load_resources)(struct rendergraph_node* self);
    b8 (*execute)(struct rendergraph_node* self, struct frame_data* p_frame_data);
    void (*destroy)(struct rendergraph_node* self);
} rendergraph_node;

struct rg_dep_graph;

typedef struct rendergraph
{
    char* name;

    // A pointer to the global colorbuffer framebuffer
    struct texture* global_colorbuffer;
    // A pointer to the global depthbuffer framebuffer
    struct texture* global_depthbuffer;

    u32 node_count;
    // Array of nodes in this graph
    rendergraph_node* nodes;

    rendergraph_node* begin_node;
    rendergraph_node* end_node;

    u32* execution_list;

    struct rg_dep_graph* dep_graph;
} rendergraph;

typedef struct rendergraph_node_sink_config
{
    const char* name;
    const char* type;
    const char* source_name;
} rendergraph_node_sink_config;

typedef struct rendergraph_node_config
{
    const char* name;
    const char* type;

    u32 sink_count;
    rendergraph_node_sink_config* sinks;

    const char* config_str;
} rendergraph_node_config;

typedef struct rendergraph_node_factory
{
    const char* type;
    b8 (*create)(rendergraph* graph, rendergraph_node* node, const struct rendergraph_node_config* config);
} rendergraph_node_factory;

BAPI b8 rendergraph_create(const char* config_str, struct texture* global_colorbuffer, struct texture* global_depthbuffer, rendergraph* out_graph);
BAPI void rendergraph_destroy(rendergraph* graph);

BAPI b8 rendergraph_finalize(rendergraph* graph);
BAPI b8 rendergraph_initialize(rendergraph* graph);
BAPI b8 rendergraph_load_resources(rendergraph* graph);
BAPI b8 rendergraph_execute_frame(rendergraph* graph, struct frame_data* p_frame_data);

BAPI rendergraph_resource_type string_to_resource_type(const char* str);

b8 rendergraph_system_initialize(u64* memory_requirement, struct rendergraph_system_state* state);
void rendergraph_system_shutdown(struct rendergraph_system_state* state);

BAPI b8 rendergraph_system_node_factory_register(struct rendergraph_system_state* state, const rendergraph_node_factory* new_factory);

#endif
