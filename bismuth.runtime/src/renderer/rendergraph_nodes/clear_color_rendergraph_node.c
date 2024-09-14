#include "clear_color_rendergraph_node.h"

#include "core/engine.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "renderer/renderer_frontend.h"
#include "renderer/rendergraph.h"
#include "strings/bstring.h"

typedef struct clear_color_rendergraph_node_config
{
    const char* source_name;
} clear_color_rendergraph_node_config;

typedef struct clear_color_rendergraph_node_internal_data
{
    struct renderer_system_state* renderer;
    struct bresource_texture* buffer_texture;
} clear_color_rendergraph_node_internal_data;

static b8 deserialize_config(const char* source_str, clear_color_rendergraph_node_config* out_config);
static void destroy_config(clear_color_rendergraph_node_config* config);

b8 clear_color_rendergraph_node_create(struct rendergraph* graph, struct rendergraph_node* self, const struct rendergraph_node_config* config)
{
    if (!graph || !self)
        return false;

    // This node does require the config string
    clear_color_rendergraph_node_config typed_config = {0};
    if (!deserialize_config(config->config_str, &typed_config))
    {
        BERROR("Failed to deserialize configuration for clear_color_rendergraph_node. Node creation failed");
        return false;
    }

    self->internal_data = ballocate(sizeof(clear_color_rendergraph_node_internal_data), MEMORY_TAG_RENDERER);
    clear_color_rendergraph_node_internal_data* internal_data = self->internal_data;
    internal_data->renderer = engine_systems_get()->renderer_system;

    self->name = string_duplicate(config->name);

    // Has one sink, for the colorbuffer
    self->sink_count = 1;
    self->sinks = ballocate(sizeof(rendergraph_sink) * self->sink_count, MEMORY_TAG_ARRAY);

    // Setup the colorbuffer sink
    rendergraph_sink* colorbuffer_sink = &self->sinks[0];
    colorbuffer_sink->name = string_duplicate("colorbuffer");
    colorbuffer_sink->type = RENDERGRAPH_RESOURCE_TYPE_TEXTURE;
    colorbuffer_sink->bound_source = 0;
    // Save off the configured source name for later lookup and binding
    colorbuffer_sink->configured_source_name = string_duplicate(typed_config.source_name);

    // Has one source, for the colorbuffer
    self->source_count = 1;
    self->sources = ballocate(sizeof(rendergraph_source) * self->source_count, MEMORY_TAG_ARRAY);

    // Setup the colorbuffer source
    rendergraph_source* colorbuffer_source = &self->sources[0];
    colorbuffer_source->name = string_duplicate("colorbuffer");
    colorbuffer_source->type = RENDERGRAPH_RESOURCE_TYPE_TEXTURE;
    colorbuffer_source->value.t = graph->global_colorbuffer;
    colorbuffer_source->is_bound = false;

    // Function pointers
    self->initialize = clear_color_rendergraph_node_initialize;
    self->destroy = clear_color_rendergraph_node_destroy;
    self->load_resources = clear_color_rendergraph_node_load_resources;
    self->execute = clear_color_rendergraph_node_execute;

    destroy_config(&typed_config);

    return true;
}

b8 clear_color_rendergraph_node_initialize(struct rendergraph_node* self)
{
    // Nothing to initialize here, this is a no-op
    return true;
}

b8 clear_color_rendergraph_node_load_resources(struct rendergraph_node* self)
{
    if (!self)
        return false;

    clear_color_rendergraph_node_internal_data* internal_data = self->internal_data;
    internal_data->buffer_texture = self->sinks[0].bound_source->value.t;

    return true;
}

b8 clear_color_rendergraph_node_execute(struct rendergraph_node* self, struct frame_data* p_frame_data)
{
    if (!self)
        return false;

    clear_color_rendergraph_node_internal_data* internal_data = self->internal_data;

    return renderer_clear_color(internal_data->renderer, internal_data->buffer_texture->renderer_texture_handle);
}

void clear_color_rendergraph_node_destroy(struct rendergraph_node* self)
{
    if (self)
    {
        if (self->name)
        {
            string_free(self->name);
            self->name = 0;
        }

        if (self->source_count && self->sources)
        {
            bfree(self->sources, sizeof(rendergraph_source) * self->source_count, MEMORY_TAG_ARRAY);
            self->sources = 0;
            self->source_count = 0;
        }

        if (self->sink_count && self->sinks)
        {
            bfree(self->sinks, sizeof(rendergraph_sink) * self->sink_count, MEMORY_TAG_ARRAY);
            self->sinks = 0;
            self->sink_count = 0;
        }
    }
}

b8 clear_color_rendergraph_node_register_factory(void)
{
    rendergraph_node_factory factory = {0};
    factory.type = "clear_color";
    factory.create = clear_color_rendergraph_node_create;
    return rendergraph_system_node_factory_register(engine_systems_get()->rendergraph_system, &factory);
}

static b8 deserialize_config(const char* source_str, clear_color_rendergraph_node_config* out_config)
{
    if (!source_str || !string_length(source_str) || !out_config)
        return false;

    bson_tree tree = {0};
    if (!bson_tree_from_string(source_str, &tree))
    {
        BERROR("Failed to parse config for clear_color_rendergraph_node");
        return false;
    }

    b8 result = bson_object_property_value_get_string(&tree.root, "source_name", &out_config->source_name);
    if (!result)
        BERROR("Failed to read required config property 'source_name' from config. Deserialization failed");

    bson_tree_cleanup(&tree);

    return result;
}

static void destroy_config(clear_color_rendergraph_node_config* config)
{
    if (config)
    {
        if (config->source_name)
        {
            string_free(config->source_name);
            config->source_name = 0;
        }
    }
}
