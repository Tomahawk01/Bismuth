#include "basset_importer_static_mesh_obj.h"

#include <assets/basset_types.h>
#include <core/engine.h>
#include <core_render_types.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <platform/vfs.h>
#include <serializers/basset_binary_static_mesh_serializer.h>
#include <serializers/basset_material_serializer.h>
#include <strings/bname.h>
#include <strings/bstring.h>

#include "math/bmath.h"
#include "serializers/obj_mtl_serializer.h"
#include "serializers/obj_serializer.h"
#include "strings/bstring_id.h"

b8 basset_importer_static_mesh_obj_import(const struct basset_importer* self, u64 data_size, const void* data, void* params, struct basset* out_asset)
{
    if (!self || !data_size || !data)
    {
        BERROR("basset_importer_static_mesh_obj_import requires valid pointers to self and data, as well as a nonzero data_size");
        return false;
    }
    basset_static_mesh* typed_asset = (basset_static_mesh*)out_asset;
    const char* material_file_name = 0;

    struct vfs_state* vfs = engine_systems_get()->vfs_system_state;

    // Handle OBJ file import
    {
        obj_source_asset obj_asset = {0};
        if (!obj_serializer_deserialize(data, &obj_asset))
        {
            BERROR("OBJ file import failed! See logs for details");
            return false;
        }

        // Convert OBJ asset to static_mesh

        // Header-level data
        typed_asset->geometry_count = obj_asset.geometry_count;
        typed_asset->center = obj_asset.center;
        typed_asset->extents = obj_asset.extents;
        typed_asset->geometries = ballocate(sizeof(basset_static_mesh_geometry) * typed_asset->geometry_count, MEMORY_TAG_ARRAY);

        // Each geometry
        for (u32 i = 0; i < typed_asset->geometry_count; ++i)
        {
            obj_source_geometry* g_src = &obj_asset.geometries[i];
            basset_static_mesh_geometry* g = &typed_asset->geometries[i];

            // Take copies of all data
            g->center = g_src->center;
            g->extents = g_src->extents;

            if (g_src->name)
                g->name = bname_create(g_src->name);

            if (g_src->material_asset_name)
                g->material_asset_name = bname_create(g_src->material_asset_name);

            if (g_src->index_count && g_src->indices)
            {
                u64 index_size = sizeof(u32) * g_src->index_count;
                g->index_count = g_src->index_count;
                g->indices = ballocate(index_size, MEMORY_TAG_ARRAY);
                bcopy_memory(g->indices, g_src->indices, index_size);
            }

            if (g_src->vertex_count && g_src->vertices)
            {
                u64 vertex_size = sizeof(vertex_3d) * g_src->vertex_count;
                g->vertex_count = g_src->vertex_count;
                g->vertices = ballocate(vertex_size, MEMORY_TAG_ARRAY);
                bcopy_memory(g->vertices, g_src->vertices, vertex_size);
            }
        }

        // Save off a copy of the string so the OBJ asset can be let go
        material_file_name = string_duplicate(obj_asset.material_file_name);

        // Cleanup OBJ asset
        for (u32 i = 0; i < obj_asset.geometry_count; ++i)
        {
            obj_source_geometry* g_src = &obj_asset.geometries[i];

            if (g_src->name)
                string_free(g_src->name);

            if (g_src->material_asset_name)
                string_free(g_src->material_asset_name);

            if (g_src->index_count && g_src->indices)
                bfree(g_src->indices, sizeof(u32) * g_src->index_count, MEMORY_TAG_ARRAY);

            if (g_src->vertex_count && g_src->vertices)
                bfree(g_src->vertices, sizeof(vertex_3d) * g_src->vertex_count, MEMORY_TAG_ARRAY);
        }
        bfree(obj_asset.geometries, sizeof(obj_source_geometry) * obj_asset.geometry_count, MEMORY_TAG_ARRAY);
        if (obj_asset.material_file_name)
            string_free(obj_asset.material_file_name);
    }

    // Serialize static_mesh and write out bsm file
    {
        u64 serialized_size = 0;
        void* serialized_data = basset_binary_static_mesh_serialize(out_asset, &serialized_size);
        if (!serialized_data || !serialized_size)
        {
            BERROR("Failed to serialize binary static mesh");
            return false;
        }

        // Write out .bsm file
        if (!vfs_asset_write(vfs, out_asset, true, serialized_size, serialized_data))
            BWARN("Failed to write .bsm file. See logs for details. Static mesh asset still imported and can be used");
    }

    // Deserialize the material file, if there is one
    {
        obj_mtl_source_asset mtl_asset = {0};
        if (material_file_name)
        {
            // Build path based on OBJ file path. The files should sit together on disk
            const char* obj_path = bstring_id_string_get(out_asset->meta.source_asset_path);
            char path_buf[512] = {0};
            string_directory_from_path(path_buf, obj_path);
            const char* mtl_path = string_format("%s%s", path_buf, material_file_name);

            vfs_asset_data mtl_file_data = {0};
            vfs_request_direct_from_disk_sync(vfs, mtl_path, false, 0, 0, &mtl_file_data);
            if (mtl_file_data.result == VFS_REQUEST_RESULT_SUCCESS)
            {
                // Deserialize the mtl file
                if (!obj_mtl_serializer_deserialize(mtl_file_data.text, &mtl_asset))
                {
                    // NOTE: Intentionally not aborting here because the mesh can still be uses sans materials
                    BWARN("Failed to parse MTL file data. See logs for details");
                }
                else
                {
                    for (u32 i = 0; i < mtl_asset.material_count; ++i)
                    {
                        obj_mtl_source_material* m_src = &mtl_asset.materials[i];

                        // Convert to basset_material
                        basset_material new_material = {0};

                        // Set material name and package name
                        new_material.base.name = m_src->name;
                        new_material.base.package_name = out_asset->package_name;
                        // Since it's an import, make note of the source asset path as well
                        new_material.base.meta.source_asset_path = bstring_id_create(mtl_path);

                        // Imports do not use a custom shader
                        new_material.custom_shader_name = 0;

                        new_material.type = m_src->type;
                        new_material.model = m_src->model;

                        // Force defaults for things not considered in OBJ MTL files
                        new_material.casts_shadow = true;
                        new_material.recieves_shadow = true;

                        // Transparency - if there is a transparency "map" (which is usually the same as the ambient/diffuse map) or
                        // the material is non-opaque (i.e ) less than 1.0f, then it should be marked as transparent
                        new_material.has_transparency = false; // m_src->diffuse_transparency_image_asset_name || m_src->diffuse_transparency < 1.0f;

                        // Material maps
                        // Base color
                        if (new_material.model == BMATERIAL_MODEL_PBR)
                        {
                            // Base color translates from diffuse only for PBR
                            if (m_src->diffuse_image_asset_name)
                            {
                                new_material.base_color_map.resource_name = m_src->diffuse_image_asset_name;
                                new_material.base_color_map.package_name = out_asset->package_name;
                            }
                            new_material.base_color = vec4_from_vec3(m_src->diffuse_color, 1.0f);

                            // Metallic
                            if (m_src->metallic_image_asset_name)
                            {
                                new_material.metallic_map.resource_name = m_src->metallic_image_asset_name;
                                new_material.metallic_map.package_name = out_asset->package_name;
                                // NOTE: Always assume red channel for OBJ MTL imports
                                new_material.metallic_map.channel = TEXTURE_CHANNEL_R;
                            }
                            new_material.metallic = m_src->metallic;

                            // Roughness
                            if (m_src->roughness_image_asset_name)
                            {
                                new_material.roughness_map.resource_name = m_src->roughness_image_asset_name;
                                new_material.roughness_map.package_name = out_asset->package_name;
                                // NOTE: Always assume red channel for OBJ MTL imports
                                new_material.roughness_map.channel = TEXTURE_CHANNEL_R;
                            }
                            new_material.roughness = m_src->roughness;

                            // Ambient occlusion NOTE: not supported for OBJ MTL imports
                            new_material.ambient_occlusion_enabled = false;
                            new_material.ambient_occlusion = 1.0;

                            // MRA (combined Metallic/Roughness/AO maps)
                            if (m_src->mra_image_asset_name)
                            {
                                new_material.mra_map.resource_name = m_src->mra_image_asset_name;
                                new_material.mra_map.package_name = out_asset->package_name;
                                new_material.use_mra = true;

                                // In this one scenario, enable AO since the MRA map can provide it
                                new_material.ambient_occlusion_enabled = true;
                            }
                            else if (new_material.metallic_map.resource_name != INVALID_BNAME && new_material.metallic_map.resource_name == new_material.roughness_map.resource_name == new_material.ambient_occlusion_map.resource_name)
                            {
                                // If metallic, roughness and ao all point to the same texture (and there is a texture), switch to MRA instead
                                new_material.mra_map.resource_name = new_material.metallic_map.resource_name;
                                new_material.mra_map.package_name = new_material.metallic_map.resource_name;
                                new_material.use_mra = true;

                                // In this one scenario, enable AO since the MRA map can provide it
                                new_material.ambient_occlusion_enabled = true;
                            }
                            else
                            {
                                new_material.use_mra = false;
                            }
                        }
                        else if (new_material.model == BMATERIAL_MODEL_PHONG)
                        {
                            // TODO: make use of the ambient color map
                            if (m_src->ambient_image_asset_name)
                                BWARN("Material has ambient color map set, but will not be imported due to engine limitations");

                            if (m_src->diffuse_image_asset_name)
                            {
                                new_material.base_color_map.resource_name = m_src->diffuse_image_asset_name;
                                new_material.base_color_map.package_name = out_asset->package_name;
                            }
                            // For phong, base color is ambient + diffuse
                            new_material.base_color = vec4_from_vec3(vec3_add(m_src->ambient_color, m_src->diffuse_color), 1.0f);

                            // Specular - only used for phong
                            if (m_src->specular_image_asset_name)
                            {
                                new_material.specular_color_map.resource_name = m_src->specular_image_asset_name;
                                new_material.specular_color_map.package_name = out_asset->package_name;
                            }
                            new_material.specular_color = vec4_from_vec3(m_src->specular_color, 1.0f);
                        }

                        // Normal
                        if (m_src->normal_image_asset_name)
                        {
                            new_material.normal_map.resource_name = m_src->normal_image_asset_name;
                            new_material.normal_map.package_name = out_asset->package_name;
                            new_material.normal_enabled = true;
                        }
                        else
                        {
                            new_material.normal_enabled = false;
                        }

                        // Emissive
                        if (m_src->emissive_image_asset_name)
                        {
                            new_material.emissive_map.resource_name = m_src->emissive_image_asset_name;
                            new_material.emissive_map.package_name = out_asset->package_name;
                        }
                        new_material.emissive = vec4_from_vec3(m_src->emissive_color, 1.0f);

                        // Serialize the material
                        const char* serialized_text = basset_material_serialize((basset*)&new_material);
                        if (!serialized_text)
                            BWARN("Failed to serialize material '%s'. See logs for details", bname_string_get(new_material.base.name));

                        // Write out bmt file
                        if (!vfs_asset_write(vfs, (basset*)&new_material, false, string_length(serialized_text), serialized_text))
                            BERROR("Failed to write serialized material to disk");
                    } // each material

                    // Cleanup materials
                    if (mtl_asset.material_count)
                    {
                        BFREE_TYPE_CARRAY(mtl_asset.materials, obj_mtl_source_material, mtl_asset.material_count);
                    }
                }
            } // success
        }
    } // end material processing

    return true;
}
