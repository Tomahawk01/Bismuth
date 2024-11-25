#pragma once

#include "bresources/bresource_types.h"
#include "systems/material_system.h"

typedef struct static_mesh_instance
{
    /** @brief A randomly-generated identifier specific to this instance */
    u64 instance_id;

    /** @brief A constant pointer to the underlying mesh resource */
    const bresource_static_mesh* mesh_resource;

    /**
     * @brief An array of handles to material instances associated with the submeshes.
     * Elements match up to mesh_resource->submeshes index-wise. Thus the count of this array is the same as mesh_resource->submesh_count
     */
    bhandle* material_instances;

    vec4 tint;
} static_mesh_instance;

/** @brief Defines flags used for rendering static meshes */
typedef enum static_mesh_render_data_flag
{
    /** @brief Indicates that the winding order for the given static mesh should be inverted */
    STATICM_ESH_RENDER_DATA_FLAG_WINDING_INVERTED_BIT = 0x0001
} staticm_esh_render_data_flag;

/**
 * @brief Collection of flags for a static mesh submesh to be rendered
 * @see static_mesh_render_data_flag
 */
typedef u32 static_mesh_render_data_flag_bits;

/**
 * @brief The render data for an individual static sub-mesh to be rendered
 */
typedef struct static_mesh_submesh_render_data
{
    /** @brief Flags for the static mesh to be rendered */
    static_mesh_render_data_flag_bits flags;

    /** @brief The vertex count */
    u32 vertex_count;
    /** @brief The offset from the beginning of the vertex buffer */
    u64 vertex_buffer_offset;

    /** @brief The index count */
    u32 index_count;
    /** @brief The offset from the beginning of the index buffer */
    u64 index_buffer_offset;

    /** @brief The instance of the material to use with this static mesh when rendering */
    // FIXME: Provide a copy of relevant material/material instance data here, not just a handle to it
    // material_instance material;
} static_mesh_submesh_render_data;

/**
 * Contains data required to render a static mesh (ultimately its submeshes)
 */
typedef struct static_mesh_render_data
{
    /** The identifier of the mesh instance being rendered */
    u64 instance_id;

    /** @brief The number of submeshes to be rendered */
    u32 submesh_count;
    /** @brief The array of submeshes to be rendered */
    static_mesh_submesh_render_data* submeshes;

    /** The index of the Image-Based-Lighting probe to be used, if applicable */
    u8 ibl_probe_index;

    /** @brief The tint override to be used when rendering all submeshes. Typically white (1, 1, 1, 1) if not used */
    vec4 tint;
} static_mesh_render_data;

struct static_mesh_system_state;

BAPI b8 static_mesh_system_initialize(u64* memory_requirement, struct static_mesh_system_state* state);
BAPI void static_mesh_system_shutdown(struct static_mesh_system_state* state);

BAPI b8 static_mesh_system_instance_acquire(struct static_mesh_system_state* state, bname name, bname resource_name, static_mesh_instance* out_instance);
BAPI void static_mesh_system_instance_release(struct static_mesh_system_state* state, static_mesh_instance* instance);

BAPI b8 static_mesh_system_render_data_generate(const static_mesh_instance* instance, static_mesh_render_data_flag_bits flags, static_mesh_render_data* out_render_data);
BAPI void static_mesh_system_render_data_destroy(static_mesh_render_data* render_data);
