#include "skybox.h"

#include "core/engine.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "math/geometry.h"
#include "renderer/renderer_frontend.h"
#include "strings/bname.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"
#include <runtime_defines.h>

b8 skybox_create(skybox_config config, skybox* out_skybox)
{
    if (!out_skybox)
    {
        BERROR("skybox_create requires a valid pointer to out_skybox!");
        return false;
    }

    out_skybox->cubemap_name = bname_create(config.cubemap_name);
    out_skybox->state = SKYBOX_STATE_CREATED;
    out_skybox->cubemap = 0;

    return true;
}

b8 skybox_initialize(skybox* sb)
{
    if (!sb)
    {
        BERROR("skybox_initialize requires a valid pointer to sb!");
        return false;
    }

    sb->group_id = INVALID_ID;

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

    sb->geometry = geometry_generate_cube(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, sb->cubemap_name);
    if (!renderer_geometry_upload(&sb->geometry))
    {
        BERROR("Failed to upload skybox geometry");
    }

    sb->cubemap = texture_system_request_cube(sb->cubemap_name, true, false, 0, 0);

    sb->skybox_shader_group_data_generation = INVALID_ID_U16;
    sb->skybox_shader_draw_data_generation = INVALID_ID_U16;

    bhandle skybox_shader = shader_system_get(bname_create(SHADER_NAME_RUNTIME_SKYBOX), bname_create(PACKAGE_NAME_RUNTIME)); // TODO: allow configurable shader
    if (!renderer_shader_per_group_resources_acquire(engine_systems_get()->renderer_system, skybox_shader, &sb->group_id))
    {
        BFATAL("Unable to acquire shader per-group resources for skybox");
        return false;
    }
    if (!renderer_shader_per_draw_resources_acquire(engine_systems_get()->renderer_system, skybox_shader, &sb->draw_id))
    {
        BFATAL("Unable to acquire shader per-draw resources for skybox");
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

    bhandle skybox_shader = shader_system_get(bname_create(SHADER_NAME_RUNTIME_SKYBOX), bname_create(PACKAGE_NAME_RUNTIME)); // TODO: allow configurable shader
    if (!renderer_shader_per_group_resources_release(engine_systems_get()->renderer_system, skybox_shader, sb->group_id))
    {
        BWARN("Unable to release shader group resources for skybox");
        return false;
    }

    sb->skybox_shader_group_data_generation = INVALID_ID_U16;

    renderer_geometry_destroy(&sb->geometry);
    geometry_destroy(&sb->geometry);

    if (sb->cubemap_name)
    {
        if (sb->cubemap)
        {
            texture_system_release_resource((bresource_texture*)sb->cubemap);
            sb->cubemap = 0;
        }

        sb->cubemap_name = 0;
    }

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
    if (sb->group_id != INVALID_ID)
    {
        b8 result = skybox_unload(sb);
        if (!result)
        {
            BERROR("skybox_destroy() - Failed to successfully unload skybox before destruction");
        }
    }
}
