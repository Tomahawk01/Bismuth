#include "mesh.h"
#include "core/bmemory.h"
#include "core/logger.h"
#include "core/identifier.h"
#include "systems/job_system.h"
#include "systems/resource_system.h"
#include "systems/geometry_system.h"
#include "renderer/renderer_types.inl"

typedef struct mesh_load_params
{
    const char* resource_name;
    mesh* out_mesh;
    resource mesh_resource;
} mesh_load_params;

void mesh_load_job_success(void* params)
{
    mesh_load_params* mesh_params = (mesh_load_params*)params;

    // This also handles the GPU upload
    geometry_config* configs = (geometry_config*)mesh_params->mesh_resource.data;
    mesh_params->out_mesh->geometry_count = mesh_params->mesh_resource.data_size;
    mesh_params->out_mesh->geometries = ballocate(sizeof(geometry*) * mesh_params->out_mesh->geometry_count, MEMORY_TAG_ARRAY);
    for (u32 i = 0; i < mesh_params->out_mesh->geometry_count; ++i)
        mesh_params->out_mesh->geometries[i] = geometry_system_acquire_from_config(configs[i], true);
    mesh_params->out_mesh->generation++;

    BTRACE("Successfully loaded mesh '%s'", mesh_params->resource_name);

    resource_system_unload(&mesh_params->mesh_resource);
}

void mesh_load_job_fail(void* params)
{
    mesh_load_params* mesh_params = (mesh_load_params*)params;

    BERROR("Failed to load mesh '%s'", mesh_params->resource_name);

    resource_system_unload(&mesh_params->mesh_resource);
}

b8 mesh_load_job_start(void* params, void* result_data)
{
    mesh_load_params* load_params = (mesh_load_params*)params;
    b8 result = resource_system_load(load_params->resource_name, RESOURCE_TYPE_MESH, 0, &load_params->mesh_resource);

    // NOTE: Load params are also used as result data here, only mesh_resource field is populated now
    bcopy_memory(result_data, load_params, sizeof(mesh_load_params));

    return result;
}

static b8 mesh_load_from_resource(const char* resource_name, mesh* out_mesh)
{
    out_mesh->generation = INVALID_ID_U8;

    mesh_load_params params;
    params.resource_name = resource_name;
    params.out_mesh = out_mesh;
    params.mesh_resource = (resource){};

    job_info job = job_create(mesh_load_job_start, mesh_load_job_success, mesh_load_job_fail, &params, sizeof(mesh_load_params), sizeof(mesh_load_params));
    job_system_submit(job);

    return true;
}

b8 mesh_create(mesh_config config, mesh* out_mesh)
{
    if (!out_mesh)
        return false;

    bzero_memory(out_mesh, sizeof(mesh));

    out_mesh->config = config;
    out_mesh->generation = INVALID_ID_U8;
    return true;
}

b8 mesh_initialize(mesh* m)
{
    if (!m)
        return false;

    if (m->config.resource_name)
    {
        return true;
    }
    else
    {
        if (!m->config.g_configs)
            return false;

        m->geometry_count = m->config.geometry_count;
        m->geometries = ballocate(sizeof(geometry*), MEMORY_TAG_ARRAY);
    }
    return true;
}

b8 mesh_load(mesh* m)
{
    if (!m)
        return false;

    m->unique_id = identifier_aquire_new_id(m);

    if (m->config.resource_name)
    {
        return mesh_load_from_resource(m->config.resource_name, m);
    }
    else
    {
        if (!m->config.g_configs)
            return false;

        for (u32 i = 0; i < m->config.geometry_count; ++i)
        {
            m->geometries[i] = geometry_system_acquire_from_config(m->config.g_configs[i], true);
            m->generation = 0;

            // Clean up allocations for geometry config
            // TODO: Do this during unload/destroy
            geometry_system_config_dispose(&m->config.g_configs[i]);
        }
    }

    return true;
}

b8 mesh_unload(mesh* m)
{
    if (m)
    {
        for (u32 i = 0; i < m->geometry_count; ++i)
            geometry_system_release(m->geometries[i]);

        bfree(m->geometries, sizeof(geometry*) * m->geometry_count, MEMORY_TAG_ARRAY);
        bzero_memory(m, sizeof(mesh));

        m->generation = INVALID_ID_U8;

        return true;
    }

    return false;
}

b8 mesh_destroy(mesh* m)
{
    if (!m)
        return false;

    if (m->geometries)
    {
        if (!mesh_unload(m))
        {
            BERROR("mesh_destroy - failed to unload mesh");
            return false;
        }
    }
    return true;
}
