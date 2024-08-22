#include "skybox_rendergraph_node.h"

#include "core/engine.h"
#include "identifiers/bhandle.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.h"
#include "renderer/rendergraph.h"
#include "resources/skybox.h"
#include "strings/bstring.h"
#include "systems/shader_system.h"

typedef struct skybox_shader_locations
{
    u16 projection_location;
    u16 view_location;
    u16 cube_map_location;
} skybox_shader_locations;

typedef struct skybox_renderpass_node_internal_data
{
    struct renderer_system_state* renderer;

    shader* s;
    u32 shader_id;
    skybox_shader_locations locations;

    struct texture* colorbuffer_texture;

    skybox* sb;

    viewport vp;
    mat4 view;
    mat4 projection;
} skybox_rendergraph_node_internal_data;

BAPI b8 skybox_rendergraph_node_create(struct rendergraph* graph, struct rendergraph_node* self, const struct rendergraph_node_config* config)
{
    if (!self)
    {
        BERROR("skybox_rendergraph_node_create requires a valid pointer to a pass");
        return false;
    }
    if (!config)
    {
        BERROR("skybox_rendergraph_node_create requires a valid configuration");
        return false;
    }

    // Setup internal data
    self->internal_data = ballocate(sizeof(skybox_rendergraph_node_internal_data), MEMORY_TAG_RENDERER);
    skybox_rendergraph_node_internal_data* internal_data = self->internal_data;
    internal_data->renderer = engine_systems_get()->renderer_system;

    self->name = string_duplicate(config->name);

    // Has one sink, for the colorbuffer
    self->sink_count = 1;
    self->sinks = ballocate(sizeof(rendergraph_sink) * self->sink_count, MEMORY_TAG_ARRAY);

    rendergraph_node_sink_config* sink_config = 0;
    for (u32 i = 0; i < config->sink_count; ++i)
    {
        rendergraph_node_sink_config* sink = &config->sinks[i];
        if (strings_equali("colorbuffer", sink->name))
        {
            sink_config = sink;
            break;
        }
    }

    if (sink_config)
    {
        // Setup the colorbuffer sink
        rendergraph_sink* colorbuffer_sink = &self->sinks[0];
        colorbuffer_sink->name = string_duplicate("colorbuffer");
        colorbuffer_sink->type = RENDERGRAPH_RESOURCE_TYPE_TEXTURE;
        colorbuffer_sink->bound_source = 0;
        // Save off the configured source name for later lookup and binding
        colorbuffer_sink->configured_source_name = string_duplicate(sink_config->source_name);
    }
    else
    {
        BERROR("Skybox rendergraph node requires configuration for sink called 'colorbuffer'");
        return false;
    }

    // Has one source, for the colorbuffer
    self->source_count = 1;
    self->sources = ballocate(sizeof(rendergraph_source) * self->source_count, MEMORY_TAG_ARRAY);

    // Setup the colorbuffer source
    rendergraph_source* colorbuffer_source = &self->sources[0];
    colorbuffer_source->name = string_duplicate("colorbuffer");
    colorbuffer_source->type = RENDERGRAPH_RESOURCE_TYPE_TEXTURE;
    colorbuffer_source->value.t = 0;
    colorbuffer_source->is_bound = false;

    // Function pointers
    self->initialize = skybox_rendergraph_node_initialize;
    self->destroy = skybox_rendergraph_node_destroy;
    self->load_resources = skybox_rendergraph_node_load_resources;
    self->execute = skybox_rendergraph_node_execute;

    return true;
}

b8 skybox_rendergraph_node_initialize(struct rendergraph_node* self)
{
    if (!self)
        return false;

    skybox_rendergraph_node_internal_data* internal_data = self->internal_data;

    // Load skybox shader
    // Get a pointer to the shader
    internal_data->s = shader_system_get("Shader.Builtin.Skybox");
    internal_data->shader_id = internal_data->s->id;
    internal_data->locations.projection_location = shader_system_uniform_location(internal_data->shader_id, "projection");
    internal_data->locations.view_location = shader_system_uniform_location(internal_data->shader_id, "view");
    internal_data->locations.cube_map_location = shader_system_uniform_location(internal_data->shader_id, "cube_texture");

    return true;
}

b8 skybox_rendergraph_node_load_resources(struct rendergraph_node* self)
{
    if (!self)
        return false;

    // Resolve framebuffer handle via bound source
    skybox_rendergraph_node_internal_data* internal_data = self->internal_data;
    if (self->sinks[0].bound_source)
    {
        internal_data->colorbuffer_texture = self->sinks[0].bound_source->value.t;
        self->sources[0].value.t = internal_data->colorbuffer_texture;
        self->sources[0].is_bound = true;
        return true;
    }

    return false;
}

b8 skybox_rendergraph_node_execute(struct rendergraph_node* self, struct frame_data* p_frame_data)
{
    if (!self)
        return false;

    skybox_rendergraph_node_internal_data* internal_data = self->internal_data;

    // Bind the viewport
    renderer_active_viewport_set(&internal_data->vp);

    renderer_begin_rendering(internal_data->renderer, p_frame_data, 1, &internal_data->colorbuffer_texture->renderer_texture_handle, b_handle_invalid(), 0);

    if (internal_data->sb && internal_data->sb->g->generation != INVALID_ID_U16)
    {
        shader_system_use_by_id(internal_data->shader_id);

        // Get the view matrix, but zero out the position so the skybox stays put on screen
        mat4 view_matrix = internal_data->view;
        view_matrix.data[12] = 0.0f;
        view_matrix.data[13] = 0.0f;
        view_matrix.data[14] = 0.0f;

        // Apply globals
        if (!shader_system_uniform_set_by_location(internal_data->shader_id, internal_data->locations.projection_location, &internal_data->projection))
        {
            BERROR("Failed to apply skybox projection uniform");
            return false;
        }
        if (!shader_system_uniform_set_by_location(internal_data->shader_id, internal_data->locations.view_location, &view_matrix))
        {
            BERROR("Failed to apply skybox view uniform");
            return false;
        }
        shader_system_apply_global(internal_data->shader_id);

        // Instance
        shader_system_bind_instance(internal_data->shader_id, internal_data->sb->instance_id);
        if (!shader_system_uniform_set_by_location(internal_data->shader_id, internal_data->locations.cube_map_location, &internal_data->sb->cubemap))
        {
            BERROR("Failed to apply skybox cube map uniform");
            return false;
        }

        shader_system_apply_instance(internal_data->shader_id);

        // Draw it
        geometry_render_data render_data = {};
        render_data.material = internal_data->sb->g->material;
        render_data.vertex_count = internal_data->sb->g->vertex_count;
        render_data.vertex_element_size = internal_data->sb->g->vertex_element_size;
        render_data.vertex_buffer_offset = internal_data->sb->g->vertex_buffer_offset;
        render_data.index_count = internal_data->sb->g->index_count;
        render_data.index_element_size = internal_data->sb->g->index_element_size;
        render_data.index_buffer_offset = internal_data->sb->g->index_buffer_offset;

        renderer_geometry_draw(&render_data);
    }

    renderer_end_rendering(internal_data->renderer, p_frame_data);

    return true;
}

void skybox_rendergraph_node_destroy(struct rendergraph_node* self)
{
    if (self)
    {
        if (self->internal_data)
        {
            // skybox_rendergraph_node_internal_data* internal_data = self->internal_data;
            bfree(self->internal_data, sizeof(skybox_rendergraph_node_internal_data), MEMORY_TAG_RENDERER);
            self->internal_data = 0;
        }
    }
}

void skybox_rendergraph_node_set_skybox(struct rendergraph_node* self, struct skybox* sb)
{
    if (self)
    {
        skybox_rendergraph_node_internal_data* internal_data = self->internal_data;
        internal_data->sb = sb;
    }
}

void skybox_rendergraph_node_set_viewport_and_matrices(struct rendergraph_node* self, viewport vp, mat4 view, mat4 projection)
{
    if (self)
    {
        if (self->internal_data)
        {
            skybox_rendergraph_node_internal_data* internal_data = self->internal_data;
            internal_data->vp = vp;
            internal_data->view = view;
            internal_data->projection = projection;
        }
    }
}

b8 skybox_rendergraph_node_register_factory(void)
{
    rendergraph_node_factory factory = {0};
    factory.type = "skybox";
    factory.create = skybox_rendergraph_node_create;
    return rendergraph_system_node_factory_register(engine_systems_get()->rendergraph_system, &factory);
}
