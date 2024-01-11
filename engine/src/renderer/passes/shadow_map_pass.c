#include "shadow_map_pass.h"

#include "containers/darray.h"
#include "core/bmemory.h"
#include "core/bstring.h"
#include "core/logger.h"
#include "defines.h"
#include "math/bmath.h"
#include "math/math_types.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.h"
#include "renderer/rendergraph.h"
#include "renderer/viewport.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

typedef struct shadow_map_shader_locations
{
    u16 projections_location;
    u16 views_location;
    u16 model_location;
    u32 cascade_index_location;
    u16 color_map_location;
} shadow_map_shader_locations;

typedef struct cascade_resources
{
    // One target per frame
    render_target* targets;
} cascade_resources;

typedef struct shadow_shader_instance_data
{
    u64 render_frame_number;
    u8 render_draw_index;
} shadow_shader_instance_data;

typedef struct shadow_map_pass_internal_data
{
    shadow_map_pass_config config;

    shader* s;
    shadow_map_shader_locations locations;

    viewport camera_viewport;

    texture* depth_textures;

    // One per cascade
    cascade_resources cascades[MAX_CASCADE_COUNT];

    // Track instance updates per frame
    b8* instance_updated;
    u32 instance_count;
    // Default map to be used when materials aren't available
    texture_map default_color_map;
    u32 default_instance_id;
    u64 default_instance_frame_number;
    u8 default_instance_draw_index;

    // Track instance data per instance. darray
    shadow_shader_instance_data* instances;

    // Separate shader/instance info for terrains
    shader* ts;
    shadow_map_shader_locations terrain_locations;
    texture_map default_terrain_color_map;
    u32 terrain_instance_id;
    u64 terrain_instance_frame_number;
    u8 terrain_instance_draw_index;
} shadow_map_pass_internal_data;

b8 shadow_map_pass_create(struct rendergraph_pass* self, void* config)
{
    if (!self || !config)
    {
        BERROR("shadow_map_pass_create requires both a pointer to self and a valid config");
        return false;
    }

    self->internal_data = ballocate(sizeof(shadow_map_pass_internal_data), MEMORY_TAG_RENDERER);
    shadow_map_pass_internal_data* internal_data = self->internal_data;
    internal_data->config = *((shadow_map_pass_config*)config);

    self->pass_data.ext_data = ballocate(sizeof(shadow_map_pass_extended_data), MEMORY_TAG_RENDERER);

    // Custom function pointers
    self->attachment_populate = shadow_map_pass_attachment_populate;
    self->source_populate = shadow_map_pass_source_populate;

    return true;
}

b8 shadow_map_pass_initialize(struct rendergraph_pass* self)
{
    if (!self)
        return false;

    shadow_map_pass_internal_data* internal_data = self->internal_data;

    // Create the depth attachments, one per frame
    u8 frame_count = renderer_window_attachment_count_get();

    internal_data->depth_textures = ballocate(sizeof(texture) * frame_count, MEMORY_TAG_RENDERER);

    for (u8 i = 0; i < frame_count; ++i)
    {
        // Depth
        texture* dt = &internal_data->depth_textures[i];
        dt->type = TEXTURE_TYPE_2D_ARRAY;
        dt->flags |= TEXTURE_FLAG_DEPTH | TEXTURE_FLAG_IS_WRITEABLE;
        dt->width = internal_data->config.resolution;
        dt->height = internal_data->config.resolution;
        dt->array_size = MAX_CASCADE_COUNT;
        string_format(dt->name, "shadowmap_pass_res_%u_idx_%u_depth_texture", internal_data->config.resolution, i);
        dt->mip_levels = 1;
        dt->channel_count = 4;
        dt->generation = INVALID_ID;
        renderer_texture_create_writeable(dt);
    }

    // Setup the renderpass
    renderpass_config shadowmap_pass_config = {0};
    shadowmap_pass_config.name = "Renderpass.Shadowmap";
    shadowmap_pass_config.clear_color = (vec4){0.0f, 0.0f, 0.2f, 1.0f};
    shadowmap_pass_config.clear_flags = RENDERPASS_CLEAR_DEPTH_BUFFER_FLAG;
    shadowmap_pass_config.depth = 1.0f;
    shadowmap_pass_config.stencil = 0;
    shadowmap_pass_config.target.attachment_count = 1;
    shadowmap_pass_config.target.attachments = ballocate(sizeof(render_target_attachment_config) * shadowmap_pass_config.target.attachment_count, MEMORY_TAG_ARRAY);
    shadowmap_pass_config.render_target_count = frame_count;

    // Depth attachment
    render_target_attachment_config* shadowpass_target_depth = &shadowmap_pass_config.target.attachments[0];
    shadowpass_target_depth->type = RENDER_TARGET_ATTACHMENT_TYPE_DEPTH;
    shadowpass_target_depth->source = RENDER_TARGET_ATTACHMENT_SOURCE_SELF;  // This owns the attachment
    shadowpass_target_depth->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
    shadowpass_target_depth->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
    shadowpass_target_depth->present_after = true;

    if (!renderer_renderpass_create(&shadowmap_pass_config, &self->pass))
    {
        BERROR("Shadowmap rendergraph pass - Failed to create shadow map renderpass");
        return false;
    }

    // Load shadowmap shader. Attempt to get the already-loaded shader if it doesn't exist
    const char* shadowmap_shader_name = "Shader.Shadowmap";
    
    internal_data->s = shader_system_get(shadowmap_shader_name);
    if (!internal_data->s)
    {
        BTRACE("Shader '%s' doesn't exist. Attempting to load it...", shadowmap_shader_name);
        resource shadowmap_shader_config_resource;
        if (!resource_system_load(shadowmap_shader_name, RESOURCE_TYPE_SHADER, 0, &shadowmap_shader_config_resource))
        {
            BERROR("Failed to load shadowmap shader resource");
            return false;
        }
        shader_config* shadowmap_shader_config = (shader_config*)shadowmap_shader_config_resource.data;
        if (!shader_system_create(&self->pass, shadowmap_shader_config))
        {
            BERROR("Failed to create shadowmap shader");
            return false;
        }

        resource_system_unload(&shadowmap_shader_config_resource);
        // Get a pointer to the shader
        internal_data->s = shader_system_get(shadowmap_shader_name);
    }
    else
    {
        BTRACE("Shader '%s' already exists, using it", shadowmap_shader_name);
    }
    internal_data->locations.projections_location = shader_system_uniform_location(internal_data->s, "projections");
    internal_data->locations.views_location = shader_system_uniform_location(internal_data->s, "views");
    internal_data->locations.model_location = shader_system_uniform_location(internal_data->s, "model");
    internal_data->locations.cascade_index_location = shader_system_uniform_location(internal_data->s, "cascade_index");
    internal_data->locations.color_map_location = shader_system_uniform_location(internal_data->s, "color_map");

    // Terrain shadowmap shader
    const char* terrain_shadowmap_shader_name = "Shader.ShadowmapTerrain";
    internal_data->ts = shader_system_get(terrain_shadowmap_shader_name);
    if (!internal_data->ts)
    {
        BTRACE("Shader '%s' doesn't exist. Attempting to load it...", terrain_shadowmap_shader_name);
        resource terrain_shadowmap_shader_config_resource;
        if (!resource_system_load(terrain_shadowmap_shader_name, RESOURCE_TYPE_SHADER, 0, &terrain_shadowmap_shader_config_resource))
        {
            BERROR("Failed to load terrain shadowmap shader resource");
            return false;
        }
        shader_config* terrain_shadowmap_shader_config = (shader_config*)terrain_shadowmap_shader_config_resource.data;
        if (!shader_system_create(&self->pass, terrain_shadowmap_shader_config))
        {
            BERROR("Failed to create terrain shadowmap shader");
            return false;
        }

        resource_system_unload(&terrain_shadowmap_shader_config_resource);
        // Get a pointer to the shader
        internal_data->ts = shader_system_get(terrain_shadowmap_shader_name);
    }
    else
    {
        BTRACE("Shader '%s' already exists, using it", terrain_shadowmap_shader_name);
    }

    internal_data->terrain_locations.projections_location = shader_system_uniform_location(internal_data->ts, "projections");
    internal_data->terrain_locations.views_location = shader_system_uniform_location(internal_data->ts, "views");
    internal_data->terrain_locations.model_location = shader_system_uniform_location(internal_data->ts, "model");
    internal_data->terrain_locations.cascade_index_location = shader_system_uniform_location(internal_data->ts, "cascade_index");
    internal_data->terrain_locations.color_map_location = shader_system_uniform_location(internal_data->ts, "color_map");

    return true;
}

b8 shadow_map_pass_load_resources(struct rendergraph_pass* self)
{
    if (!self)
        return false;

    shadow_map_pass_internal_data* internal_data = self->internal_data;

    // Create a texture map to be used across the board for the diffuse/albedo transparency sample
    internal_data->default_color_map.mip_levels = 1;
    internal_data->default_color_map.generation = INVALID_ID;
    internal_data->default_color_map.repeat_u = internal_data->default_color_map.repeat_v = internal_data->default_color_map.repeat_w = TEXTURE_REPEAT_CLAMP_TO_EDGE;
    internal_data->default_color_map.filter_minify = internal_data->default_color_map.filter_magnify = TEXTURE_FILTER_MODE_LINEAR;

    // Grab the default texture for the default texture map
    internal_data->default_color_map.texture = texture_system_get_default_diffuse_texture();

    // Create a texture map to be used across the board for the diffuse/albedo transparency sample
    internal_data->default_terrain_color_map.mip_levels = 1;
    internal_data->default_terrain_color_map.generation = INVALID_ID;
    internal_data->default_terrain_color_map.repeat_u = internal_data->default_terrain_color_map.repeat_v = internal_data->default_terrain_color_map.repeat_w = TEXTURE_REPEAT_CLAMP_TO_EDGE;
    internal_data->default_terrain_color_map.filter_minify = internal_data->default_terrain_color_map.filter_magnify = TEXTURE_FILTER_MODE_LINEAR;

    // Grab the default texture for the default terrain texture map
    internal_data->default_terrain_color_map.texture = texture_system_get_default_diffuse_texture();

    // Acquire resources for the default texture map
    if (!renderer_texture_map_resources_acquire(&internal_data->default_color_map))
    {
        BERROR("Failed to acquire texture map resources for default color map in shadowmap pass");
        return false;
    }

    // Acquire resources for the default terrain texture map
    if (!renderer_texture_map_resources_acquire(&internal_data->default_terrain_color_map))
    {
        BERROR("Failed to acquire texture map resources for default terrain color map in shadowmap pass");
        return false;
    }

    // Reserve an instance id for the default "material" to render to
    {
        texture_map* maps[1] = {&internal_data->default_color_map};
        shader* s = internal_data->s;
        u16 atlas_location = s->uniforms[s->instance_sampler_indices[0]].index;
        shader_instance_resource_config instance_resource_config = {0};
        // Map count for this type is known
        shader_instance_uniform_texture_config color_texture = {0};
        color_texture.uniform_location = atlas_location;
        color_texture.texture_map_count = 1;
        color_texture.texture_maps = maps;

        instance_resource_config.uniform_config_count = 1;
        instance_resource_config.uniform_configs = &color_texture;
        renderer_shader_instance_resources_acquire(internal_data->s, &instance_resource_config, &internal_data->default_instance_id);
    }

    // Reserve an instance id for the default "material" to render to
    {
        texture_map* terrain_maps[1] = {&internal_data->default_terrain_color_map};
        shader* s = internal_data->ts;
        u16 atlas_location = s->uniforms[s->instance_sampler_indices[0]].index;
        shader_instance_resource_config instance_resource_config = {0};
        // Map count for this type is known
        shader_instance_uniform_texture_config color_texture = {0};
        color_texture.uniform_location = atlas_location;
        color_texture.texture_map_count = 1;
        color_texture.texture_maps = terrain_maps;

        instance_resource_config.uniform_config_count = 1;
        instance_resource_config.uniform_configs = &color_texture;

        renderer_shader_instance_resources_acquire(internal_data->ts, &instance_resource_config, &internal_data->terrain_instance_id);
    }

    // Setup default viewport
    vec4 viewport_rect = {0, 0, internal_data->config.resolution, internal_data->config.resolution};
    if (!viewport_create(viewport_rect, 0.0f, 0.0f, 0.0f, RENDERER_PROJECTION_MATRIX_TYPE_ORTHOGRAPHIC, &internal_data->camera_viewport))
    {
        BERROR("Failed to create viewport for shadow map pass");
        return false;
    }

    // Create the depth attachments, one per frame
    u8 frame_count = renderer_window_attachment_count_get();
    // Renderpass attachments
    for (u32 i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
        cascade_resources* cascade = &internal_data->cascades[i];
        // Targets per frame
        cascade->targets = ballocate(sizeof(render_target) * frame_count, MEMORY_TAG_ARRAY);
        for (u32 f = 0; f < frame_count; ++f)
        {
            // One render target per pass
            render_target* target = &cascade->targets[f];
            target->attachment_count = 1;
            // 2 attachments per target, (color, depth)
            target->attachments = ballocate(sizeof(render_target_attachment) * target->attachment_count, MEMORY_TAG_ARRAY);
            target->attachments[0].type = RENDER_TARGET_ATTACHMENT_TYPE_DEPTH;
            target->attachments[0].source = RENDER_TARGET_ATTACHMENT_SOURCE_SELF;
            target->attachments[0].texture = &internal_data->depth_textures[f];
            target->attachments[0].present_after = true;
            target->attachments[0].load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
            target->attachments[0].store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;

            // Create the underlying render target
            renderer_render_target_create(
                target->attachment_count,
                target->attachments,
                &self->pass,
                internal_data->config.resolution,
                internal_data->config.resolution,
                i,
                target);
        }
    }

    return true;
}

b8 shadow_map_pass_execute(struct rendergraph_pass* self, struct frame_data* p_frame_data)
{
    if (!self)
        return false;

    shadow_map_pass_internal_data* internal_data = self->internal_data;
    shadow_map_pass_extended_data* ext_data = self->pass_data.ext_data;

    // Bind the internal viewport
    renderer_active_viewport_set(&internal_data->camera_viewport);

    for (u32 p = 0; p < MAX_CASCADE_COUNT; ++p)
    {
        shadow_map_cascade_data* cascade = &ext_data->cascades[p];
        if (!renderer_renderpass_begin(&self->pass, &internal_data->cascades[p].targets[p_frame_data->render_target_index]))
        {
            BERROR("Shadowmap pass failed to start");
            return false;
        }

        // Use the standard shadowmap shader
        shader_system_use_by_id(internal_data->s->id);

        // Apply globals, once per cascade
        b8 needs_update = p == 0;
        if (needs_update)
        {
            renderer_shader_bind_globals(internal_data->s);
            for (u32 i = 0; i < MAX_CASCADE_COUNT; ++i)
            {
                if (!shader_system_uniform_set_by_location_arrayed(internal_data->locations.projections_location, i, &ext_data->cascades[i].projection))
                {
                    BERROR("Failed to apply shadowmap projection uniform");
                    return false;
                }
                if (!shader_system_uniform_set_by_location_arrayed(internal_data->locations.views_location, i, &ext_data->cascades[i].view))
                {
                    BERROR("Failed to apply shadowmap view uniform");
                    return false;
                }
            }
        }

        shader_system_apply_global(needs_update, p_frame_data);

        u32 geometry_count = cascade->geometry_count;
        u32 terrain_geometry_count = cascade->terrain_geometry_count;

        // Verify enough instance resources for this frame
        u32 highest_id = 0;
        for (u32 i = 0; i < geometry_count; ++i)
        {
            material* m = cascade->geometries[i].material;
            if (m->internal_id > highest_id)
            {
                // NOTE: +1 to account for the first id being taken by the default instance
                highest_id = m->internal_id + 1;
            }
        }

        highest_id++;

        if (highest_id > internal_data->instance_count)
        {
            if (internal_data->instances)
                darray_destroy(internal_data->instances);
            internal_data->instances = darray_reserve(shadow_shader_instance_data, highest_id + 1);
            // Get more resources if needed, starting at the previous high point
            for (u32 i = internal_data->instance_count; i < highest_id; i++)
            {
                u32 instance_id;

                // Use same map for all
                texture_map* maps[1] = {&internal_data->default_color_map};
                shader* s = internal_data->s;
                u16 atlas_location = s->uniforms[s->instance_sampler_indices[0]].index;
                shader_instance_resource_config instance_resource_config = {0};
                // Map count for this type is known
                shader_instance_uniform_texture_config color_texture = {0};
                color_texture.uniform_location = atlas_location;
                color_texture.texture_map_count = 1;
                color_texture.texture_maps = maps;

                instance_resource_config.uniform_config_count = 1;
                instance_resource_config.uniform_configs = &color_texture;
                renderer_shader_instance_resources_acquire(internal_data->s, &instance_resource_config, &instance_id);

                shadow_shader_instance_data* instance = &internal_data->instances[instance_id];
                instance->render_frame_number = INVALID_ID_U64;
                instance->render_draw_index = INVALID_ID_U8;
            }
            internal_data->instance_count = highest_id;
        }

        // Static geometries
        for (u32 i = 0; i < geometry_count; ++i)
        {
            geometry_render_data* g = &cascade->geometries[i];

            u32 bind_id = INVALID_ID;
            texture_map* bind_map = 0;
            u64* render_number = 0;
            u8* draw_index = 0;

            // Decide what bindings to use
            if (g->material && g->material->maps)
            {
                // Use current material's internal id
                // NOTE: +1 to account for the first id being taken by the default instance
                bind_id = g->material->internal_id + 1;
                // Use the current material's diffuse/albedo map
                bind_map = &g->material->maps[0];
                // NOTE: can't update material's frame number/draw index because it still needs to be used for the actual scene render
                shadow_shader_instance_data* instance = &internal_data->instances[g->material->internal_id + 1];
                render_number = &instance->render_frame_number;
                draw_index = &instance->render_draw_index;
            }
            else
            {
                // Use the default instance
                bind_id = internal_data->default_instance_id;
                // Use the default color map
                bind_map = &internal_data->default_color_map;
                render_number = &internal_data->default_instance_frame_number;
                draw_index = &internal_data->default_instance_draw_index;
            }

            b8 needs_update = *render_number != p_frame_data->renderer_frame_number || *draw_index != p_frame_data->draw_index;

            // Use the bindings
            shader_system_bind_instance(bind_id);
            if (!shader_system_uniform_set_by_location(internal_data->locations.color_map_location, bind_map))
            {
                BERROR("Failed to apply shadowmap color_map uniform to static geometry");
                return false;
            }
            shader_system_apply_instance(needs_update, p_frame_data);

            // Sync the frame number and draw index
            *render_number = p_frame_data->renderer_frame_number;
            *draw_index = p_frame_data->draw_index;

            // Apply locals
            shader_system_bind_local();
            shader_system_uniform_set_by_location(internal_data->locations.model_location, &g->model);
            shader_system_uniform_set_by_location(internal_data->locations.cascade_index_location, &p);
            shader_system_apply_local(p_frame_data);

            // Invert if needed
            if (cascade->geometries[i].winding_inverted)
                renderer_winding_set(RENDERER_WINDING_CLOCKWISE);

            // Draw it
            renderer_geometry_draw(g);

            // Change back if needed
            if (cascade->geometries[i].winding_inverted)
                renderer_winding_set(RENDERER_WINDING_COUNTER_CLOCKWISE);
        }
        // Terrain - use the special terrain shadowmap shader
        shader_system_use_by_id(internal_data->ts->id);

        // Apply globals, once per cascade
        renderer_shader_bind_globals(internal_data->ts);
        if (needs_update)
        {
            for (u32 i = 0; i < MAX_CASCADE_COUNT; ++i)
            {
                // NOTE: using the internal projection matrix, not one passed in
                if (!shader_system_uniform_set_by_location_arrayed(internal_data->terrain_locations.projections_location, i, &ext_data->cascades[i].projection))
                {
                    BERROR("Failed to apply terrain shadowmap projection uniform");
                    return false;
                }
                if (!shader_system_uniform_set_by_location_arrayed(internal_data->terrain_locations.views_location, i, &ext_data->cascades[i].view))
                {
                    BERROR("Failed to apply terrain shadowmap view uniform");
                    return false;
                }
            }
        }
        shader_system_apply_global(needs_update, p_frame_data);

        for (u32 i = 0; i < terrain_geometry_count; ++i)
        {
            geometry_render_data* terrain = &cascade->terrain_geometries[i];

            // Just draw these using the default instance and texture map
            texture_map* bind_map = &internal_data->default_terrain_color_map;
            u64* render_number = &internal_data->terrain_instance_frame_number;
            u8* draw_index = &internal_data->terrain_instance_draw_index;

            b8 needs_update = *render_number != p_frame_data->renderer_frame_number || *draw_index != p_frame_data->draw_index;

            shader_system_bind_instance(internal_data->terrain_instance_id);
            if (!shader_system_uniform_set_by_location(internal_data->terrain_locations.color_map_location, bind_map))
            {
                BERROR("Failed to apply shadowmap color_map uniform to terrain geometry");
                return false;
            }
            shader_system_apply_instance(needs_update, p_frame_data);

            // Sync the frame number and draw index
            *render_number = p_frame_data->renderer_frame_number;
            *draw_index = p_frame_data->draw_index;

            // Apply the locals
            shader_system_bind_local();
            shader_system_uniform_set_by_location(internal_data->terrain_locations.model_location, &terrain->model);
            shader_system_uniform_set_by_location(internal_data->terrain_locations.cascade_index_location, &p);
            shader_system_apply_local(p_frame_data);

            // Draw it
            renderer_geometry_draw(terrain);
        }

        if (!renderer_renderpass_end(&self->pass))
        {
            BERROR("Shadowmap pass failed to end");
            return false;
        }
    }  // End cascade pass

    return true;
}

void shadow_map_pass_destroy(struct rendergraph_pass* self)
{
    if (self)
    {
        if (self->internal_data)
        {
            shadow_map_pass_internal_data* internal_data = self->internal_data;

            // Destroy the attachments, one per frame
            u8 attachment_count = renderer_window_attachment_count_get();

            // Renderpass attachments
            for (u32 i = 0; i < MAX_CASCADE_COUNT; ++i)
            {
                cascade_resources* cascade = &internal_data->cascades[i];
                // Targets per frame
                for (u32 f = 0; f < attachment_count; ++f)
                {
                    // One render target per pass
                    render_target* target = &cascade->targets[f];

                    // Create the underlying render target
                    renderer_render_target_destroy(target, true);
                }
                bfree(cascade->targets, sizeof(render_target) * attachment_count, MEMORY_TAG_ARRAY);
            }

            for (u8 i = 0; i < attachment_count; ++i)
            {
                renderer_texture_destroy(&internal_data->depth_textures[i]);
            }
            bfree(internal_data->depth_textures, sizeof(texture*) * attachment_count, MEMORY_TAG_ARRAY);

            renderer_texture_map_resources_release(&internal_data->default_color_map);
            renderer_texture_map_resources_release(&internal_data->default_terrain_color_map);
            renderer_shader_instance_resources_release(internal_data->s, internal_data->default_instance_id);
            renderer_shader_instance_resources_release(internal_data->ts, internal_data->terrain_instance_id);

            // Destroy the extended data
            if (self->pass_data.ext_data)
            {
                bfree(self->pass_data.ext_data, sizeof(shadow_map_pass_extended_data), MEMORY_TAG_RENDERER);
                self->pass_data.ext_data = 0;
            }

            // Destroy the pass
            renderer_renderpass_destroy(&self->pass);
            bfree(self->internal_data, sizeof(shadow_map_pass_internal_data), MEMORY_TAG_RENDERER);
            self->internal_data = 0;
        }
    }
}

b8 shadow_map_pass_source_populate(struct rendergraph_pass* self, rendergraph_source* source)
{
    if (!self || !source)
        return false;

    shadow_map_pass_internal_data* internal_data = self->internal_data;
    u32 frame_count = renderer_window_attachment_count_get();
    if (!source->textures)
        source->textures = ballocate(sizeof(texture*) * frame_count, MEMORY_TAG_ARRAY);

    if (strings_equali(source->name, "depthbuffer"))
    {
        for (u32 i = 0; i < frame_count; ++i)
        {
            source->textures[i] = &internal_data->depth_textures[i];
        }
        return true;
    }
    BERROR("shadow_map_pass_source_populate could not populate source '%s' as it is unrecognized", source->name);
    return false;
}

b8 shadow_map_pass_attachment_populate(struct rendergraph_pass* self, render_target_attachment* attachment)
{
    shadow_map_pass_internal_data* internal_data = self->internal_data;

    if (attachment->type == RENDER_TARGET_ATTACHMENT_TYPE_DEPTH)
    {
        attachment->texture = &internal_data->depth_textures[0];
        return true;
    }

    return false;
}
