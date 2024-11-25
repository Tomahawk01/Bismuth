#include "static_mesh_system.h"
#include "assets/basset_types.h"
#include "core/engine.h"
#include "defines.h"
#include "bresource_system.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "strings/bname.h"
#include "systems/material_system.h"

typedef struct static_mesh_system_state
{
    u32 dummy;
} static_mesh_system_state;

typedef struct static_mesh_asset_request_listener
{
    // A pointer to the static mesh instance
    static_mesh_instance* instance;
} static_mesh_resource_request_listener;

// Invoked when the resource is loaded (or if already loaded and immediately returned)
static void static_mesh_on_resource_loaded(bresource* resource, void* listener);

b8 static_mesh_system_initialize(u64* memory_requirement, struct static_mesh_system_state* state)
{
    if (!memory_requirement)
        return false;

    *memory_requirement = sizeof(static_mesh_system_state);

    if (!state)
        return true;

    state->dummy = 69;

    BDEBUG("Static mesh system initialized");

    return true;
}

void static_mesh_system_shutdown(struct static_mesh_system_state* state)
{
    if (state)
    {
        // TODO: shut down
    }
}

b8 static_mesh_system_instance_acquire(struct static_mesh_system_state* state, bname name, bname resource_name, static_mesh_instance* out_instance)
{
    if (!state || resource_name == INVALID_BNAME || !out_instance)
        return false;

    const engine_system_states* systems = engine_systems_get();

    // Setup a listener
    static_mesh_resource_request_listener* listener = BALLOC_TYPE(static_mesh_resource_request_listener, MEMORY_TAG_RESOURCE);

    bresource_request_info request = {0};
    request.type = BRESOURCE_TYPE_STATIC_MESH;
    request.assets = array_bresource_asset_info_create(1);
    request.assets.data[0].type = BASSET_TYPE_STATIC_MESH;
    request.assets.data[0].asset_name = resource_name;
    request.assets.data[0].package_name = INVALID_BNAME;
    // Setup a listener and callback
    request.listener_inst = listener;
    request.user_callback = static_mesh_on_resource_loaded;

    // Request the resource
    out_instance->mesh_resource = (bresource_static_mesh*)bresource_system_request(
        systems->bresource_state,
        resource_name,
        &request);
    out_instance->instance_id = brandom_u64();
    out_instance->tint = vec4_one(); // white

    return true;
}

void static_mesh_system_instance_release(struct static_mesh_system_state* state, static_mesh_instance* instance)
{
    struct material_system_state* material_system = engine_systems_get()->material_system;

    // Release material instances
    for (u32 i = 0; i < instance->mesh_resource->submesh_count; ++i)
        material_release_instance(material_system, &instance->material_instances[i]);

    // Cleanup the instance itself
    BFREE_TYPE_CARRAY(instance->material_instances, material_instance, instance->mesh_resource->submesh_count);
    instance->material_instances = 0;
    instance->instance_id = INVALID_ID_U64;
    instance->tint = vec4_zero();

    // Release the resource reference
    bresource_system_release(engine_systems_get()->bresource_state, instance->mesh_resource->base.name);
    instance->mesh_resource = 0;
}

b8 static_mesh_system_render_data_generate(const static_mesh_instance* instance, static_mesh_render_data_flag_bits flags, static_mesh_render_data* out_render_data)
{
    if (!instance || !out_render_data)
        return false;

    out_render_data->tint = instance->tint;
    out_render_data->instance_id = instance->instance_id;
    out_render_data->submesh_count = instance->mesh_resource->submesh_count;
    out_render_data->ibl_probe_index = 0; // TODO: This should be provided elsewhere
    // FIXME: Need a way to filter down this list by view frustum if we want that granular control
    // For now though either every submesh gets rendered when this is called, or this isn't called and nothing is rendered
    out_render_data->submeshes = BALLOC_TYPE_CARRAY(static_mesh_submesh_render_data, out_render_data->submesh_count);
    for (u32 i = 0; i < out_render_data->submesh_count; ++i)
    {
        static_mesh_submesh* submesh = &instance->mesh_resource->submeshes[i];
        static_mesh_submesh_render_data* submesh_rd = &out_render_data->submeshes[i];

        submesh_rd->material = instance->material_instances[i];
        submesh_rd->vertex_count = submesh->geometry.vertex_count;
        submesh_rd->vertex_buffer_offset = submesh->geometry.vertex_buffer_offset;
        submesh_rd->index_count = submesh->geometry.index_count;
        submesh_rd->index_buffer_offset = submesh->geometry.index_buffer_offset;
        // TODO: Need a way to provide these flags per submesh
        submesh_rd->flags = flags;
    }

    return true;
}

void static_mesh_system_render_data_destroy(static_mesh_render_data* render_data)
{
    if (render_data)
    {
        if (render_data->submeshes)
            BFREE_TYPE_CARRAY(render_data->submeshes, static_mesh_submesh_render_data, render_data->submesh_count);
    }
    bzero_memory(render_data, sizeof(static_mesh_submesh_render_data));
}

static void static_mesh_on_resource_loaded(bresource* resource, void* listener)
{
    static_mesh_resource_request_listener* typed_listener = (static_mesh_resource_request_listener*)listener;
    bresource_static_mesh* typed_resource = (bresource_static_mesh*)resource;

    if (typed_resource->submesh_count < 1)
    {
        BERROR("Static mesh resource has no submeshes. Nothing to be done");
        return;
    }

    // Request material instances for this static mesh instance
    typed_listener->instance->material_instances = BALLOC_TYPE_CARRAY(material_instance, typed_listener->instance->mesh_resource->submesh_count);

    // Process submeshes
    for (u32 i = 0; i < typed_resource->submesh_count; ++i)
    {
        static_mesh_submesh* submesh = &typed_resource->submeshes[i];

        // Request material instance
        b8 acquisition_result = material_system_acquire(
            engine_systems_get()->material_system,
            submesh->material_name,
            &typed_listener->instance->material_instances[i]);
        if (!acquisition_result)
        {
            BWARN("Failed to load material '%s' for static mesh '%s', submesh '%s'", bname_string_get(submesh->material_name), bname_string_get(typed_resource->base.name), bname_string_get(submesh->geometry.name));
        }
    }

    // Free listener
    BFREE_TYPE(listener, static_mesh_resource_request_listener, MEMORY_TAG_RESOURCE);
}
