#include "frame_end_rendergraph_node.h"

#include "../rendergraph.h"
#include "core/engine.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "strings/bstring.h"

typedef struct frame_end_rendergraph_node_config
{
    const char* colorbuffer_source;
} frame_end_rendergraph_node_config;

static b8 deserialize_config(const char* source_str, frame_end_rendergraph_node_config* out_config);
static void destroy_config(frame_end_rendergraph_node_config* config);

b8 frame_end_rendergraph_node_create(struct rendergraph* graph, struct rendergraph_node* self, const struct rendergraph_node_config* config)
{
    if (!graph || !self)
        return false;

    // This node does require the config string to extract the source name from
    frame_end_rendergraph_node_config typed_config = {0};
    if (!deserialize_config(config->config_str, &typed_config))
    {
        BERROR("Failed to deserialize configuration for frame_end_rendergraph_node. Node creation failed");
        return false;
    }

    self->name = string_duplicate(config->name);

    // Has one sink, for the colorbuffer
    self->sink_count = 1;
    self->sinks = ballocate(sizeof(rendergraph_sink) * self->sink_count, MEMORY_TAG_ARRAY);

    // No sources
    self->source_count = 0;
    self->sources = 0;

    // Setup the colorbuffer sink
    rendergraph_sink* colorbuffer_sink = &self->sinks[0];
    colorbuffer_sink->name = string_duplicate("colorbuffer");
    colorbuffer_sink->type = RENDERGRAPH_RESOURCE_TYPE_TEXTURE;
    colorbuffer_sink->bound_source = 0;
    // Save off the configured source name for later lookup and binding
    colorbuffer_sink->configured_source_name = string_duplicate(typed_config.colorbuffer_source);

    // Throw away the parsed config
    destroy_config(&typed_config);

    // Function pointers
    self->initialize = frame_end_rendergraph_node_initialize;
    self->destroy = frame_end_rendergraph_node_destroy;
    self->load_resources = 0; // no resources to load
    self->execute = frame_end_rendergraph_node_execute;

    return true;
}

b8 frame_end_rendergraph_node_initialize(struct rendergraph_node* self)
{
    // Nothing to initialize here, this is a no-op
    return true;
}

b8 frame_end_rendergraph_node_execute(struct rendergraph_node* self, struct frame_data* p_frame_data)
{
    // TODO: This is probably where an image layout transformation should occur, instead of doing it at the renderpass level and having that worry about it
    return true;
}

void frame_end_rendergraph_node_destroy(struct rendergraph_node* self)
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

b8 frame_end_rendergraph_node_register_factory(void)
{
    rendergraph_node_factory factory = {0};
    factory.type = "frame_end";
    factory.create = frame_end_rendergraph_node_create;
    return rendergraph_system_node_factory_register(engine_systems_get()->rendergraph_system, &factory);
}

static b8 deserialize_config(const char* source_str, frame_end_rendergraph_node_config* out_config)
{
    if (!source_str || !string_length(source_str) || !out_config)
        return false;

    bson_tree tree = {0};
    if (!bson_tree_from_string(source_str, &tree))
    {
        BERROR("Failed to parse config for frame_end_rendergraph_node");
        return false;
    }

    b8 result = bson_object_property_value_get_string(&tree.root, "colorbuffer_source", &out_config->colorbuffer_source);
    if (!result)
        BERROR("Failed to read required config property 'colorbuffer_source' from config. Deserialization failed");

    bson_tree_cleanup(&tree);

    return result;
}

static void destroy_config(frame_end_rendergraph_node_config* config)
{
    if (config)
    {
        if (config->colorbuffer_source)
        {
            string_free(config->colorbuffer_source);
            config->colorbuffer_source = 0;
        }
    }
}
