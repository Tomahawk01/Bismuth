#include "skybox.h"

#include "core/engine.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "renderer/renderer_frontend.h"
#include "strings/bname.h"
#include "systems/geometry_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

b8 skybox_create(skybox_config config, skybox* out_skybox)
{
    if (!out_skybox)
    {
        BERROR("skybox_create requires a valid pointer to out_skybox!");
        return false;
    }

    out_skybox->cubemap_name = bname_create(config.cubemap_name);
    out_skybox->state = SKYBOX_STATE_CREATED;

    return true;
}

b8 skybox_initialize(skybox* sb)
{
    if (!sb)
    {
        BERROR("skybox_initialize requires a valid pointer to sb!");
        return false;
    }
    bresource_texture_map* cube_map = &sb->cubemap;
    cube_map->filter_magnify = cube_map->filter_minify = TEXTURE_FILTER_MODE_LINEAR;
    cube_map->repeat_u = cube_map->repeat_v = cube_map->repeat_w = TEXTURE_REPEAT_CLAMP_TO_EDGE;
    sb->instance_id = INVALID_ID;

    sb->g_config = geometry_system_generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, sb->cubemap_name, 0);
    // Clear material name
    sb->g_config.material_name[0] = 0;
    sb->state = SKYBOX_STATE_INITIALIZED;

    return true;
}

b8 skybox_load(skybox* sb)
{
    if (!sb)
    {
        BERROR("skybox_load requires a valid pointer to sb");
        return false;
    }
    sb->state = SKYBOX_STATE_LOADING;

    sb->cubemap.texture = texture_system_request_cube(sb->cubemap_name, true, 0, 0);
    if (!renderer_bresource_texture_map_resources_acquire(engine_systems_get()->renderer_system, &sb->cubemap))
    {
        BFATAL("Unable to acquire resources for cube map texture");
        return false;
    }

    sb->g = geometry_system_acquire_from_config(sb->g_config, true);
    sb->render_frame_number = INVALID_ID_U64;

    shader* skybox_shader = shader_system_get("Shader.Builtin.Skybox"); // TODO: allow configurable shader
    bresource_texture_map* maps[1] = {&sb->cubemap};
    // shader* s = skybox_shader;
    // u16 atlas_location = s->uniforms[s->instance_sampler_indices[0]].index;
    shader_instance_resource_config instance_resource_config = {0};
    // Map count for this type is known
    shader_instance_uniform_texture_config color_texture = {0};
    // color_texture.uniform_location = atlas_location;
    color_texture.bresource_texture_map_count = 1;
    color_texture.bresource_texture_maps = maps;

    instance_resource_config.uniform_config_count = 1;
    instance_resource_config.uniform_configs = &color_texture;
    if (!renderer_shader_instance_resources_acquire(engine_systems_get()->renderer_system, skybox_shader, &instance_resource_config, &sb->instance_id))
    {
        BFATAL("Unable to acquire shader resources for skybox texture");
        return false;
    }
    sb->state = SKYBOX_STATE_LOADED;

    return true;
}

b8 skybox_unload(skybox* sb)
{
    if (!sb)
    {
        BERROR("skybox_unload requires a valid pointer to sb");
        return false;
    }
    sb->state = SKYBOX_STATE_UNDEFINED;

    shader* skybox_shader = shader_system_get("Shader.Builtin.Skybox"); // TODO: allow configurable shader
    renderer_shader_instance_resources_release(engine_systems_get()->renderer_system, skybox_shader, sb->instance_id);
    sb->instance_id = INVALID_ID;
    renderer_bresource_texture_map_resources_release(engine_systems_get()->renderer_system, &sb->cubemap);

    sb->render_frame_number = INVALID_ID_U64;

    geometry_system_config_dispose(&sb->g_config);
    if (sb->cubemap_name)
    {
        if (sb->cubemap.texture)
        {
            texture_system_release_resource((bresource_texture*)sb->cubemap.texture);
            sb->cubemap.texture = 0;
        }

        sb->cubemap_name = 0;
    }

    geometry_system_release(sb->g);

    return true;
}

void skybox_destroy(skybox* sb)
{
    if (!sb)
    {
        BERROR("skybox_destroy requires a valid pointer to sb");
        return;
    }
    sb->state = SKYBOX_STATE_UNDEFINED;

    // If loaded, unload first, then destroy
    if (sb->instance_id != INVALID_ID)
    {
        b8 result = skybox_unload(sb);
        if (!result)
        {
            BERROR("skybox_destroy() - Failed to successfully unload skybox before destruction");
        }
    }
}
