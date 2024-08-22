#include "debug_rendergraph_node.h"

#include "core/engine.h"
#include "identifiers/bhandle.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.h"
#include "renderer/rendergraph.h"
#include "strings/bstring.h"
#include "systems/material_system.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"

typedef struct debug_shader_locations
{
    u16 projection;
    u16 view;
    u16 model;
} debug_shader_locations;

typedef struct debug_rendergraph_node_internal_data
{
    struct renderer_system_state* renderer;

    u32 color_shader_id;
    shader* color_shader;
    debug_shader_locations debug_locations;

    struct texture* colorbuffer_texture;

    struct viewport* vp;
    mat4 view;
    mat4 projection;

    u32 geometry_count;
    geometry_render_data* geometries;
} debug_rendergraph_node_internal_data;

b8 debug_rendergraph_node_create(struct rendergraph* graph, struct rendergraph_node* self, const struct rendergraph_node_config* config)
{
    if (!self)
    {
        BERROR("debug_rendergraph_node_create requires a valid pointer to a pass");
        return false;
    }
    if (!config)
    {
        BERROR("debug_rendergraph_node_create requires a valid configuration");
        return false;
    }

    // Setup internal data
    self->internal_data = ballocate(sizeof(debug_rendergraph_node_internal_data), MEMORY_TAG_RENDERER);
    debug_rendergraph_node_internal_data* internal_data = self->internal_data;
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
    self->initialize = debug_rendergraph_node_initialize;
    self->destroy = debug_rendergraph_node_destroy;
    self->load_resources = debug_rendergraph_node_load_resources;
    self->execute = debug_rendergraph_node_execute;

    return true;
}

b8 debug_rendergraph_node_initialize(struct rendergraph_node* self)
{
    if (!self)
        return false;

    debug_rendergraph_node_internal_data* internal_data = self->internal_data;

    // Load debug color3d shader and get shader uniform locations
    const char* color_shader_name = "Shader.Builtin.ColorShader3D";
    resource color_shader_config_resource;
    if (!resource_system_load(color_shader_name, RESOURCE_TYPE_SHADER, 0, &color_shader_config_resource))
    {
        BERROR("Failed to load debug color 3d shader resource");
        return false;
    }
    shader_config* color_shader_config = (shader_config*)color_shader_config_resource.data;
    if (!shader_system_create(color_shader_config))
    {
        BERROR("Failed to create color 3d shader");
        return false;
    }

    resource_system_unload(&color_shader_config_resource);
    // Get a pointer to the shader
    internal_data->color_shader = shader_system_get(color_shader_name);
    internal_data->color_shader_id = internal_data->color_shader->id;
    internal_data->debug_locations.projection = shader_system_uniform_location(internal_data->color_shader_id, "projection");
    internal_data->debug_locations.view = shader_system_uniform_location(internal_data->color_shader_id, "view");
    internal_data->debug_locations.model = shader_system_uniform_location(internal_data->color_shader_id, "model");

    return true;
}

b8 debug_rendergraph_node_load_resources(struct rendergraph_node* self)
{
    if (!self)
        return false;

    // Resolve framebuffer handle via bound source
    debug_rendergraph_node_internal_data* internal_data = self->internal_data;
    if (self->sinks[0].bound_source)
    {
        internal_data->colorbuffer_texture = self->sinks[0].bound_source->value.t;
        return true;
    }

    return false;
}

b8 debug_rendergraph_node_execute(struct rendergraph_node* self, struct frame_data* p_frame_data)
{
    if (!self)
        return false;

    debug_rendergraph_node_internal_data* internal_data = self->internal_data;

    // Bind the viewport
    renderer_active_viewport_set(internal_data->vp);

    if (internal_data->geometry_count > 0)
    {
        renderer_begin_rendering(internal_data->renderer, p_frame_data, 1, &internal_data->colorbuffer_texture->renderer_texture_handle, b_handle_invalid());

        shader_system_use_by_id(internal_data->color_shader->id);

        // Globals
        shader_system_uniform_set_by_location(internal_data->color_shader_id, internal_data->debug_locations.projection, &internal_data->projection);
        shader_system_uniform_set_by_location(internal_data->color_shader_id, internal_data->debug_locations.view, &internal_data->view);
        shader_system_apply_global(internal_data->color_shader_id);

        for (u32 i = 0; i < internal_data->geometry_count; ++i)
        {
            // NOTE: No instance-level uniforms to be set
            geometry_render_data* render_data = &internal_data->geometries[i];

            // Set model matrix
            shader_system_uniform_set_by_location(internal_data->color_shader_id, internal_data->debug_locations.model, &render_data->model);
            shader_system_apply_local(internal_data->color_shader_id);

            // Draw it
            renderer_geometry_draw(render_data);
        }

        renderer_end_rendering(internal_data->renderer, p_frame_data);
    }

    return true;
}

void debug_rendergraph_node_destroy(struct rendergraph_node* self)
{
    if (self)
    {
        if (self->internal_data)
        {
            // Destroy the pass
            bfree(self->internal_data, sizeof(debug_rendergraph_node_internal_data), MEMORY_TAG_RENDERER);
            self->internal_data = 0;
        }
    }
}

b8 debug_rendergraph_node_viewport_set(struct rendergraph_node* self, struct viewport* v)
{
    if (self && self->internal_data)
    {
        debug_rendergraph_node_internal_data* internal_data = self->internal_data;
        internal_data->vp = v;
        return true;
    }
    return false;
}

b8 debug_rendergraph_node_view_projection_set(struct rendergraph_node* self, mat4 view_matrix, vec3 view_pos, mat4 projection_matrix)
{
    if (self && self->internal_data)
    {
        debug_rendergraph_node_internal_data* internal_data = self->internal_data;
        internal_data->view = view_matrix;
        internal_data->projection = projection_matrix;
        return true;
    }
    return false;
}

b8 debug_rendergraph_node_debug_geometries_set(struct rendergraph_node* self, struct frame_data* p_frame_data, u32 geometry_count, const struct geometry_render_data* geometries)
{
    if (self && self->internal_data)
    {
        debug_rendergraph_node_internal_data* internal_data = self->internal_data;
        internal_data->geometry_count = geometry_count;
        internal_data->geometries = p_frame_data->allocator.allocate(sizeof(geometry_render_data) * geometry_count);
        bcopy_memory(internal_data->geometries, geometries, sizeof(geometry_render_data) * geometry_count);
        return true;
    }
    return false;
}

b8 debug_rendergraph_node_register_factory(void)
{
    rendergraph_node_factory factory = {0};
    factory.type = "debug";
    factory.create = debug_rendergraph_node_create;
    return rendergraph_system_node_factory_register(engine_systems_get()->rendergraph_system, &factory);
}
