#include "geometry.h"

#include "debug/bassert.h"
#include "defines.h"
#include "logger.h"
#include "math/geometry.h"
#include "math/bmath.h"
#include "math/math_types.h"
#include "memory/bmemory.h"
#include "strings/bname.h"

void geometry_generate_normals(u32 vertex_count, vertex_3d* vertices, u32 index_count, u32* indices)
{
    for (u32 i = 0; i < index_count; i += 3)
    {
        u32 i0 = indices[i + 0];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];

        vec3 edge1 = vec3_sub(vertices[i1].position, vertices[i0].position);
        vec3 edge2 = vec3_sub(vertices[i2].position, vertices[i0].position);

        vec3 normal = vec3_normalized(vec3_cross(edge1, edge2));

        vertices[i0].normal = normal;
        vertices[i1].normal = normal;
        vertices[i2].normal = normal;
    }
}

void geometry_generate_tangents(u32 vertex_count, vertex_3d* vertices, u32 index_count, u32* indices)
{
    for (u32 i = 0; i < index_count; i += 3)
    {
        u32 i0 = indices[i + 0];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];

        vec3 edge1 = vec3_sub(vertices[i1].position, vertices[i0].position);
        vec3 edge2 = vec3_sub(vertices[i2].position, vertices[i0].position);

        f32 deltaU1 = vertices[i1].texcoord.x - vertices[i0].texcoord.x;
        f32 deltaV1 = vertices[i1].texcoord.y - vertices[i0].texcoord.y;

        f32 deltaU2 = vertices[i2].texcoord.x - vertices[i0].texcoord.x;
        f32 deltaV2 = vertices[i2].texcoord.y - vertices[i0].texcoord.y;

        f32 dividend = (deltaU1 * deltaV2 - deltaU2 * deltaV1);
        f32 fc = 1.0f / dividend;

        vec3 tangent = (vec3){(fc * (deltaV2 * edge1.x - deltaV1 * edge2.x)),
                              (fc * (deltaV2 * edge1.y - deltaV1 * edge2.y)),
                              (fc * (deltaV2 * edge1.z - deltaV1 * edge2.z))};

        tangent = vec3_normalized(tangent);

        f32 sx = deltaU1, sy = deltaU2;
        f32 tx = deltaV1, ty = deltaV2;
        f32 handedness = ((tx * sy - ty * sx) < 0.0f) ? -1.0f : 1.0f;

        vec3 t4 = vec3_mul_scalar(tangent, handedness);
        vertices[i0].tangent = t4;
        vertices[i1].tangent = t4;
        vertices[i2].tangent = t4;
    }
}

b8 vertex3d_equal(vertex_3d vert_0, vertex_3d vert_1)
{
    return vec3_compare(vert_0.position, vert_1.position, B_FLOAT_EPSILON) &&
           vec3_compare(vert_0.normal, vert_1.normal, B_FLOAT_EPSILON) &&
           vec2_compare(vert_0.texcoord, vert_1.texcoord, B_FLOAT_EPSILON) &&
           vec4_compare(vert_0.color, vert_1.color, B_FLOAT_EPSILON) &&
           vec3_compare(vert_0.tangent, vert_1.tangent, B_FLOAT_EPSILON);
}

void reassign_index(u32 index_count, u32* indices, u32 from, u32 to)
{
    for (u32 i = 0; i < index_count; ++i)
    {
        if (indices[i] == from)
            indices[i] = to;
        else if (indices[i] > from)
            indices[i]--;
    }
}

void geometry_deduplicate_vertices(u32 vertex_count, vertex_3d* vertices, u32 index_count, u32* indices, u32* out_vertex_count, vertex_3d** out_vertices)
{
    // Create new arrays for the collection to sit in
    vertex_3d* unique_verts = ballocate(sizeof(vertex_3d) * vertex_count, MEMORY_TAG_ARRAY);
    *out_vertex_count = 0;

    u32 found_count = 0;
    for (u32 v = 0; v < vertex_count; ++v)
    {
        b8 found = false;
        for (u32 u = 0; u < *out_vertex_count; ++u)
        {
            if (vertex3d_equal(vertices[v], unique_verts[u]))
            {
                // Reassign indices, do _not_ copy
                reassign_index(index_count, indices, v - found_count, u);
                found = true;
                found_count++;
                break;
            }
        }

        if (!found)
        {
            // Copy over to unique
            unique_verts[*out_vertex_count] = vertices[v];
            (*out_vertex_count)++;
        }
    }

    // Allocate new vertices array
    *out_vertices = ballocate(sizeof(vertex_3d) * (*out_vertex_count), MEMORY_TAG_ARRAY);
    // Copy over unique
    bcopy_memory(*out_vertices, unique_verts, sizeof(vertex_3d) * (*out_vertex_count));
    // Destroy temp array
    bfree(unique_verts, sizeof(vertex_3d) * vertex_count, MEMORY_TAG_ARRAY);

    BDEBUG("geometry_deduplicate_vertices: removed %d vertices, orig/now %d/%d", vertex_count - *out_vertex_count, vertex_count, *out_vertex_count);
}

void generate_uvs_from_image_coords(u32 img_width, u32 img_height, u32 px_x, u32 px_y, f32* out_tx, f32* out_ty)
{
    BASSERT_DEBUG(out_tx);
    BASSERT_DEBUG(out_ty);
    *out_tx = (f32)px_x / img_width;
    *out_ty = (f32)px_y / img_height;
}

bgeometry geometry_generate_quad(f32 width, f32 height, f32 tx_min, f32 tx_max, f32 ty_min, f32 ty_max, bname name)
{
    bgeometry out_geometry = {0};

    out_geometry.name = name;
    out_geometry.type = BGEOMETRY_TYPE_2D_STATIC;
    out_geometry.generation = INVALID_ID_U16;
    out_geometry.extents.min = (vec3){-width * 0.5f, -height * 0.5f, 0.0f};
    out_geometry.extents.max = (vec3){width * 0.5f, height * 0.5f, 0.0f};
    // Always half width/height since upper left is 0,0 and lower right is width/height
    out_geometry.center = vec3_zero();
    out_geometry.vertex_element_size = sizeof(vertex_2d);
    out_geometry.vertex_count = 4;
    out_geometry.vertices = BALLOC_TYPE_CARRAY(vertex_2d, out_geometry.vertex_count);
    out_geometry.vertex_buffer_offset = INVALID_ID_U64;
    out_geometry.index_element_size = sizeof(u32);
    out_geometry.index_count = 6;
    out_geometry.indices = BALLOC_TYPE_CARRAY(u32, out_geometry.index_count);
    out_geometry.index_buffer_offset = INVALID_ID_U64;

    vertex_2d vertices[4];
    vertices[0].position.x = 0.0f;   // 0    3
    vertices[0].position.y = 0.0f;   //
    vertices[0].texcoord.x = tx_min; //
    vertices[0].texcoord.y = ty_min; // 2    1

    vertices[1].position.y = height;
    vertices[1].position.x = width;
    vertices[1].texcoord.x = tx_max;
    vertices[1].texcoord.y = ty_max;

    vertices[2].position.x = 0.0f;
    vertices[2].position.y = height;
    vertices[2].texcoord.x = tx_min;
    vertices[2].texcoord.y = ty_max;

    vertices[3].position.x = width;
    vertices[3].position.y = 0.0;
    vertices[3].texcoord.x = tx_max;
    vertices[3].texcoord.y = ty_min;
    BCOPY_TYPE_CARRAY(out_geometry.vertices, vertices, vertex_2d, out_geometry.vertex_count);

    // Indices - counter-clockwise
    u32 indices[6] = {2, 1, 0, 3, 0, 1};
    BCOPY_TYPE_CARRAY(out_geometry.indices, indices, u32, out_geometry.index_count);

    return out_geometry;
}

bgeometry geometry_generate_line2d(vec2 point_0, vec2 point_1, bname name)
{
    bgeometry out_geometry = {0};
    out_geometry.name = name;
    out_geometry.type = BGEOMETRY_TYPE_2D_STATIC;
    out_geometry.generation = INVALID_ID_U16;
    out_geometry.center = vec3_from_vec2(vec2_mid(point_0, point_1), 0.0f);
    out_geometry.extents.min = (vec3){
        BMIN(point_0.x, point_1.x),
        BMIN(point_0.y, point_1.y),
        0.0f};
    out_geometry.extents.max = (vec3){
        BMAX(point_0.x, point_1.x),
        BMAX(point_0.y, point_1.y),
        0.0f};
    out_geometry.vertex_count = 2;
    out_geometry.vertex_element_size = sizeof(vertex_2d);
    out_geometry.vertices = BALLOC_TYPE_CARRAY(vertex_2d, out_geometry.vertex_count);
    ((vertex_2d*)out_geometry.vertices)[0].position = point_0;
    ((vertex_2d*)out_geometry.vertices)[1].position = point_1;
    out_geometry.vertex_buffer_offset = INVALID_ID_U64;
    // NOTE: lines do not have indices
    out_geometry.index_count = 0;
    out_geometry.index_element_size = sizeof(u32);
    out_geometry.indices = 0;
    out_geometry.index_buffer_offset = INVALID_ID_U64;

    return out_geometry;
}

bgeometry geometry_generate_line3d(vec3 point_0, vec3 point_1, bname name)
{
    bgeometry out_geometry = {0};
    out_geometry.name = name;
    out_geometry.type = BGEOMETRY_TYPE_3D_STATIC_COLOR_ONLY;
    out_geometry.generation = INVALID_ID_U16;
    out_geometry.center = vec3_mid(point_0, point_1);
    out_geometry.extents.min = (vec3){
        BMIN(point_0.x, point_1.x),
        BMIN(point_0.y, point_1.y),
        BMIN(point_0.z, point_1.z)};
    out_geometry.extents.max = (vec3){
        BMAX(point_0.x, point_1.x),
        BMAX(point_0.y, point_1.y),
        BMAX(point_0.z, point_1.z)};
    out_geometry.vertex_count = 2;
    out_geometry.vertex_element_size = sizeof(color_vertex_3d);
    out_geometry.vertices = BALLOC_TYPE_CARRAY(color_vertex_3d, out_geometry.vertex_count);
    ((color_vertex_3d*)out_geometry.vertices)[0].position = vec4_from_vec3(point_0, 1.0f);
    ((color_vertex_3d*)out_geometry.vertices)[1].position = vec4_from_vec3(point_1, 1.0f);
    out_geometry.vertex_buffer_offset = INVALID_ID_U64;
    // NOTE: lines do not have indices
    out_geometry.index_count = 0;
    out_geometry.index_element_size = sizeof(u32);
    out_geometry.indices = 0;
    out_geometry.index_buffer_offset = INVALID_ID_U64;

    return out_geometry;
}

bgeometry geometry_generate_plane(f32 width, f32 height, u32 x_segment_count, u32 y_segment_count, f32 tile_x, f32 tile_y, bname name)
{
    if (width == 0)
    {
        BWARN("Width must be nonzero. Defaulting to one");
        width = 1.0f;
    }
    if (height == 0)
    {
        BWARN("Height must be nonzero. Defaulting to one");
        height = 1.0f;
    }
    if (x_segment_count < 1)
    {
        BWARN("x_segment_count must be a positive number. Defaulting to one");
        x_segment_count = 1;
    }
    if (y_segment_count < 1)
    {
        BWARN("y_segment_count must be a positive number. Defaulting to one");
        y_segment_count = 1;
    }

    if (tile_x == 0)
    {
        BWARN("tile_x must be nonzero. Defaulting to one");
        tile_x = 1.0f;
    }
    if (tile_y == 0)
    {
        BWARN("tile_y must be nonzero. Defaulting to one");
        tile_y = 1.0f;
    }

    f32 half_width = width * 0.5f;
    f32 half_height = height * 0.5f;

    bgeometry out_geometry = {0};
    out_geometry.name = name;
    out_geometry.type = BGEOMETRY_TYPE_3D_STATIC_COLOR_ONLY;
    out_geometry.generation = INVALID_ID_U16;
    out_geometry.extents.min = (vec3){-half_width, -half_height, 0.0f};
    out_geometry.extents.max = (vec3){half_width, half_height, 0.0f};
    // Always 0 since min/max of each axis are -/+ half of the size
    out_geometry.center = vec3_zero();
    out_geometry.vertex_element_size = sizeof(color_vertex_3d);
    out_geometry.vertex_count = x_segment_count * y_segment_count * 4; // 4 verts per segment
    out_geometry.vertices = BALLOC_TYPE_CARRAY(color_vertex_3d, out_geometry.vertex_count);
    out_geometry.vertex_buffer_offset = INVALID_ID_U64;
    out_geometry.index_element_size = sizeof(u32);
    out_geometry.index_count = x_segment_count * y_segment_count * 6; // 6 indices per segment
    out_geometry.indices = BALLOC_TYPE_CARRAY(u32, out_geometry.index_count);
    out_geometry.index_buffer_offset = INVALID_ID_U64;

    // TODO: This generates extra vertices, but we can always deduplicate them later
    f32 seg_width = width / x_segment_count;
    f32 seg_height = height / y_segment_count;
    for (u32 y = 0; y < y_segment_count; ++y)
    {
        for (u32 x = 0; x < x_segment_count; ++x)
        {
            // Generate vertices
            f32 min_x = (x * seg_width) - half_width;
            f32 min_y = (y * seg_height) - half_height;
            f32 max_x = min_x + seg_width;
            f32 max_y = min_y + seg_height;
            f32 min_uvx = (x / (f32)x_segment_count) * tile_x;
            f32 min_uvy = (y / (f32)y_segment_count) * tile_y;
            f32 max_uvx = ((x + 1) / (f32)x_segment_count) * tile_x;
            f32 max_uvy = ((y + 1) / (f32)y_segment_count) * tile_y;

            u32 v_offset = ((y * x_segment_count) + x) * 4;
            vertex_3d* v0 = &((vertex_3d*)out_geometry.vertices)[v_offset + 0];
            vertex_3d* v1 = &((vertex_3d*)out_geometry.vertices)[v_offset + 1];
            vertex_3d* v2 = &((vertex_3d*)out_geometry.vertices)[v_offset + 2];
            vertex_3d* v3 = &((vertex_3d*)out_geometry.vertices)[v_offset + 3];

            v0->position.x = min_x;
            v0->position.y = min_y;
            v0->texcoord.x = min_uvx;
            v0->texcoord.y = min_uvy;

            v1->position.x = max_x;
            v1->position.y = max_y;
            v1->texcoord.x = max_uvx;
            v1->texcoord.y = max_uvy;

            v2->position.x = min_x;
            v2->position.y = max_y;
            v2->texcoord.x = min_uvx;
            v2->texcoord.y = max_uvy;

            v3->position.x = max_x;
            v3->position.y = min_y;
            v3->texcoord.x = max_uvx;
            v3->texcoord.y = min_uvy;

            // Generate indices
            u32 i_offset = ((y * x_segment_count) + x) * 6;
            ((u32*)out_geometry.indices)[i_offset + 0] = v_offset + 0;
            ((u32*)out_geometry.indices)[i_offset + 1] = v_offset + 1;
            ((u32*)out_geometry.indices)[i_offset + 2] = v_offset + 2;
            ((u32*)out_geometry.indices)[i_offset + 3] = v_offset + 0;
            ((u32*)out_geometry.indices)[i_offset + 4] = v_offset + 3;
            ((u32*)out_geometry.indices)[i_offset + 5] = v_offset + 1;
        }
    }

    return out_geometry;
}

void geometry_recalculate_line_box3d_by_points(bgeometry* geometry, vec3 points[8])
{
    // Front lines
    {
        // top
        ((vertex_3d*)geometry->vertices)[0].position = points[2];
        ((vertex_3d*)geometry->vertices)[1].position = points[3];
        // right
        ((vertex_3d*)geometry->vertices)[2].position = points[1];
        ((vertex_3d*)geometry->vertices)[3].position = points[2];
        // bottom
        ((vertex_3d*)geometry->vertices)[4].position = points[0];
        ((vertex_3d*)geometry->vertices)[5].position = points[1];
        // left
        ((vertex_3d*)geometry->vertices)[6].position = points[3];
        ((vertex_3d*)geometry->vertices)[7].position = points[0];
    }
    // back lines
    {
        // top
        ((vertex_3d*)geometry->vertices)[8].position = points[6];
        ((vertex_3d*)geometry->vertices)[9].position = points[7];
        // right
        ((vertex_3d*)geometry->vertices)[10].position = points[5];
        ((vertex_3d*)geometry->vertices)[11].position = points[6];
        // bottom
        ((vertex_3d*)geometry->vertices)[12].position = points[4];
        ((vertex_3d*)geometry->vertices)[13].position = points[5];
        // left
        ((vertex_3d*)geometry->vertices)[14].position = points[7];
        ((vertex_3d*)geometry->vertices)[15].position = points[4];
    }

    // top connecting lines
    {
        // left
        ((vertex_3d*)geometry->vertices)[16].position = points[3];
        ((vertex_3d*)geometry->vertices)[17].position = points[7];
        // right
        ((vertex_3d*)geometry->vertices)[18].position = points[2];
        ((vertex_3d*)geometry->vertices)[19].position = points[6];
    }
    // bottom connecting lines
    {
        // left
        ((vertex_3d*)geometry->vertices)[20].position = points[0];
        ((vertex_3d*)geometry->vertices)[21].position = points[4];
        // right
        ((vertex_3d*)geometry->vertices)[22].position = points[1];
        ((vertex_3d*)geometry->vertices)[23].position = points[5];
    }
}

void geometry_recalculate_line_box3d_by_extents(bgeometry* geometry, extents_3d extents)
{
    // Front lines
    {
        // top
        ((vertex_3d*)geometry->vertices)[0].position = (vec3){extents.min.x, extents.min.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[1].position = (vec3){extents.max.x, extents.min.y, extents.min.z};
        // right
        ((vertex_3d*)geometry->vertices)[2].position = (vec3){extents.max.x, extents.min.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[3].position = (vec3){extents.max.x, extents.max.y, extents.min.z};
        // bottom
        ((vertex_3d*)geometry->vertices)[4].position = (vec3){extents.max.x, extents.max.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[5].position = (vec3){extents.min.x, extents.max.y, extents.min.z};
        // left
        ((vertex_3d*)geometry->vertices)[6].position = (vec3){extents.min.x, extents.min.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[7].position = (vec3){extents.min.x, extents.max.y, extents.min.z};
    }
    // back lines
    {
        // top
        ((vertex_3d*)geometry->vertices)[8].position = (vec3){extents.min.x, extents.min.y, extents.max.z};
        ((vertex_3d*)geometry->vertices)[9].position = (vec3){extents.max.x, extents.min.y, extents.max.z};
        // right
        ((vertex_3d*)geometry->vertices)[10].position = (vec3){extents.max.x, extents.min.y, extents.max.z};
        ((vertex_3d*)geometry->vertices)[11].position = (vec3){extents.max.x, extents.max.y, extents.max.z};
        // bottom
        ((vertex_3d*)geometry->vertices)[12].position = (vec3){extents.max.x, extents.max.y, extents.max.z};
        ((vertex_3d*)geometry->vertices)[13].position = (vec3){extents.min.x, extents.max.y, extents.max.z};
        // left
        ((vertex_3d*)geometry->vertices)[14].position = (vec3){extents.min.x, extents.min.y, extents.max.z};
        ((vertex_3d*)geometry->vertices)[15].position = (vec3){extents.min.x, extents.max.y, extents.max.z};
    }

    // top connecting lines
    {
        // left
        ((vertex_3d*)geometry->vertices)[16].position = (vec3){extents.min.x, extents.min.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[17].position = (vec3){extents.min.x, extents.min.y, extents.max.z};
        // right
        ((vertex_3d*)geometry->vertices)[18].position = (vec3){extents.max.x, extents.min.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[19].position = (vec3){extents.max.x, extents.min.y, extents.max.z};
    }
    // bottom connecting lines
    {
        // left
        ((vertex_3d*)geometry->vertices)[20].position = (vec3){extents.min.x, extents.max.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[21].position = (vec3){extents.min.x, extents.max.y, extents.max.z};
        // right
        ((vertex_3d*)geometry->vertices)[22].position = (vec3){extents.max.x, extents.max.y, extents.min.z};
        ((vertex_3d*)geometry->vertices)[23].position = (vec3){extents.max.x, extents.max.y, extents.max.z};
    }
}

bgeometry geometry_generate_line_box3d(vec3 size, bname name)
{
    f32 half_width = size.x * 0.5f;
    f32 half_height = size.y * 0.5f;
    f32 half_depth = size.z * 0.5f;

    bgeometry out_geometry = {0};
    out_geometry.name = name;
    out_geometry.type = BGEOMETRY_TYPE_3D_STATIC_COLOR_ONLY;
    out_geometry.generation = INVALID_ID_U16;
    out_geometry.extents.min = (vec3){-half_width, -half_height, -half_depth};
    out_geometry.extents.max = (vec3){half_width, half_height, half_depth};
    // Always 0 since min/max of each axis are -/+ half of the size
    out_geometry.center = vec3_zero();
    out_geometry.vertex_element_size = sizeof(color_vertex_3d);
    out_geometry.vertex_count = 2 * 12; // 12 lines to make a cube
    out_geometry.vertices = BALLOC_TYPE_CARRAY(color_vertex_3d, out_geometry.vertex_count);
    out_geometry.vertex_buffer_offset = INVALID_ID_U64;
    out_geometry.index_element_size = sizeof(u32);
    out_geometry.index_count = 6 * 6; // 6 indices per side, 6 sides
    out_geometry.indices = BALLOC_TYPE_CARRAY(u32, out_geometry.index_count);
    out_geometry.index_buffer_offset = INVALID_ID_U64;

    extents_3d extents = {0};
    extents.min.x = -half_width;
    extents.min.y = -half_height;
    extents.min.z = -half_depth;
    extents.max.x = half_width;
    extents.max.y = half_height;
    extents.max.z = half_depth;

    geometry_recalculate_line_box3d_by_extents(&out_geometry, extents);

    return out_geometry;
}

bgeometry geometry_generate_cube(f32 width, f32 height, f32 depth, f32 tile_x, f32 tile_y, bname name)
{
    if (width == 0)
    {
        BWARN("Width must be nonzero. Defaulting to one");
        width = 1.0f;
    }
    if (height == 0)
    {
        BWARN("Height must be nonzero. Defaulting to one");
        height = 1.0f;
    }
    if (depth == 0)
    {
        BWARN("Depth must be nonzero. Defaulting to one");
        depth = 1;
    }
    if (tile_x == 0)
    {
        BWARN("tile_x must be nonzero. Defaulting to one");
        tile_x = 1.0f;
    }
    if (tile_y == 0)
    {
        BWARN("tile_y must be nonzero. Defaulting to one");
        tile_y = 1.0f;
    }

    f32 half_width = width * 0.5f;
    f32 half_height = height * 0.5f;
    f32 half_depth = depth * 0.5f;

    bgeometry out_geometry = {0};
    out_geometry.name = name;
    out_geometry.type = BGEOMETRY_TYPE_3D_STATIC;
    out_geometry.generation = INVALID_ID_U16;
    out_geometry.extents.min = (vec3){-half_width, -half_height, -half_depth};
    out_geometry.extents.max = (vec3){half_width, half_height, half_depth};
    // Always 0 since min/max of each axis are -/+ half of the size
    out_geometry.center = vec3_zero();
    out_geometry.vertex_element_size = sizeof(vertex_3d);
    out_geometry.vertex_count = 4 * 6; // 4 verts per side, 6 sides
    out_geometry.vertices = BALLOC_TYPE_CARRAY(vertex_3d, out_geometry.vertex_count);
    out_geometry.vertex_buffer_offset = INVALID_ID_U64;
    out_geometry.index_element_size = sizeof(u32);
    out_geometry.index_count = 6 * 6; // 6 indices per side, 6 sides
    out_geometry.indices = BALLOC_TYPE_CARRAY(u32, out_geometry.index_count);
    out_geometry.index_buffer_offset = INVALID_ID_U64;

    f32 min_x = -half_width;
    f32 min_y = -half_height;
    f32 min_z = -half_depth;
    f32 max_x = half_width;
    f32 max_y = half_height;
    f32 max_z = half_depth;
    f32 min_uvx = 0.0f;
    f32 min_uvy = 0.0f;
    f32 max_uvx = tile_x;
    f32 max_uvy = tile_y;

    vertex_3d verts[24];

    // Front face
    verts[(0 * 4) + 0].position = (vec3){min_x, min_y, max_z};
    verts[(0 * 4) + 1].position = (vec3){max_x, max_y, max_z};
    verts[(0 * 4) + 2].position = (vec3){min_x, max_y, max_z};
    verts[(0 * 4) + 3].position = (vec3){max_x, min_y, max_z};
    verts[(0 * 4) + 0].texcoord = (vec2){min_uvx, min_uvy};
    verts[(0 * 4) + 1].texcoord = (vec2){max_uvx, max_uvy};
    verts[(0 * 4) + 2].texcoord = (vec2){min_uvx, max_uvy};
    verts[(0 * 4) + 3].texcoord = (vec2){max_uvx, min_uvy};
    verts[(0 * 4) + 0].normal = (vec3){0.0f, 0.0f, 1.0f};
    verts[(0 * 4) + 1].normal = (vec3){0.0f, 0.0f, 1.0f};
    verts[(0 * 4) + 2].normal = (vec3){0.0f, 0.0f, 1.0f};
    verts[(0 * 4) + 3].normal = (vec3){0.0f, 0.0f, 1.0f};

    // Back face
    verts[(1 * 4) + 0].position = (vec3){max_x, min_y, min_z};
    verts[(1 * 4) + 1].position = (vec3){min_x, max_y, min_z};
    verts[(1 * 4) + 2].position = (vec3){max_x, max_y, min_z};
    verts[(1 * 4) + 3].position = (vec3){min_x, min_y, min_z};
    verts[(1 * 4) + 0].texcoord = (vec2){min_uvx, min_uvy};
    verts[(1 * 4) + 1].texcoord = (vec2){max_uvx, max_uvy};
    verts[(1 * 4) + 2].texcoord = (vec2){min_uvx, max_uvy};
    verts[(1 * 4) + 3].texcoord = (vec2){max_uvx, min_uvy};
    verts[(1 * 4) + 0].normal = (vec3){0.0f, 0.0f, -1.0f};
    verts[(1 * 4) + 1].normal = (vec3){0.0f, 0.0f, -1.0f};
    verts[(1 * 4) + 2].normal = (vec3){0.0f, 0.0f, -1.0f};
    verts[(1 * 4) + 3].normal = (vec3){0.0f, 0.0f, -1.0f};

    // Left
    verts[(2 * 4) + 0].position = (vec3){min_x, min_y, min_z};
    verts[(2 * 4) + 1].position = (vec3){min_x, max_y, max_z};
    verts[(2 * 4) + 2].position = (vec3){min_x, max_y, min_z};
    verts[(2 * 4) + 3].position = (vec3){min_x, min_y, max_z};
    verts[(2 * 4) + 0].texcoord = (vec2){min_uvx, min_uvy};
    verts[(2 * 4) + 1].texcoord = (vec2){max_uvx, max_uvy};
    verts[(2 * 4) + 2].texcoord = (vec2){min_uvx, max_uvy};
    verts[(2 * 4) + 3].texcoord = (vec2){max_uvx, min_uvy};
    verts[(2 * 4) + 0].normal = (vec3){-1.0f, 0.0f, 0.0f};
    verts[(2 * 4) + 1].normal = (vec3){-1.0f, 0.0f, 0.0f};
    verts[(2 * 4) + 2].normal = (vec3){-1.0f, 0.0f, 0.0f};
    verts[(2 * 4) + 3].normal = (vec3){-1.0f, 0.0f, 0.0f};

    // Right face
    verts[(3 * 4) + 0].position = (vec3){max_x, min_y, max_z};
    verts[(3 * 4) + 1].position = (vec3){max_x, max_y, min_z};
    verts[(3 * 4) + 2].position = (vec3){max_x, max_y, max_z};
    verts[(3 * 4) + 3].position = (vec3){max_x, min_y, min_z};
    verts[(3 * 4) + 0].texcoord = (vec2){min_uvx, min_uvy};
    verts[(3 * 4) + 1].texcoord = (vec2){max_uvx, max_uvy};
    verts[(3 * 4) + 2].texcoord = (vec2){min_uvx, max_uvy};
    verts[(3 * 4) + 3].texcoord = (vec2){max_uvx, min_uvy};
    verts[(3 * 4) + 0].normal = (vec3){1.0f, 0.0f, 0.0f};
    verts[(3 * 4) + 1].normal = (vec3){1.0f, 0.0f, 0.0f};
    verts[(3 * 4) + 2].normal = (vec3){1.0f, 0.0f, 0.0f};
    verts[(3 * 4) + 3].normal = (vec3){1.0f, 0.0f, 0.0f};

    // Bottom face
    verts[(4 * 4) + 0].position = (vec3){max_x, min_y, max_z};
    verts[(4 * 4) + 1].position = (vec3){min_x, min_y, min_z};
    verts[(4 * 4) + 2].position = (vec3){max_x, min_y, min_z};
    verts[(4 * 4) + 3].position = (vec3){min_x, min_y, max_z};
    verts[(4 * 4) + 0].texcoord = (vec2){min_uvx, min_uvy};
    verts[(4 * 4) + 1].texcoord = (vec2){max_uvx, max_uvy};
    verts[(4 * 4) + 2].texcoord = (vec2){min_uvx, max_uvy};
    verts[(4 * 4) + 3].texcoord = (vec2){max_uvx, min_uvy};
    verts[(4 * 4) + 0].normal = (vec3){0.0f, -1.0f, 0.0f};
    verts[(4 * 4) + 1].normal = (vec3){0.0f, -1.0f, 0.0f};
    verts[(4 * 4) + 2].normal = (vec3){0.0f, -1.0f, 0.0f};
    verts[(4 * 4) + 3].normal = (vec3){0.0f, -1.0f, 0.0f};

    // Top face
    verts[(5 * 4) + 0].position = (vec3){min_x, max_y, max_z};
    verts[(5 * 4) + 1].position = (vec3){max_x, max_y, min_z};
    verts[(5 * 4) + 2].position = (vec3){min_x, max_y, min_z};
    verts[(5 * 4) + 3].position = (vec3){max_x, max_y, max_z};
    verts[(5 * 4) + 0].texcoord = (vec2){min_uvx, min_uvy};
    verts[(5 * 4) + 1].texcoord = (vec2){max_uvx, max_uvy};
    verts[(5 * 4) + 2].texcoord = (vec2){min_uvx, max_uvy};
    verts[(5 * 4) + 3].texcoord = (vec2){max_uvx, min_uvy};
    verts[(5 * 4) + 0].normal = (vec3){0.0f, 1.0f, 0.0f};
    verts[(5 * 4) + 1].normal = (vec3){0.0f, 1.0f, 0.0f};
    verts[(5 * 4) + 2].normal = (vec3){0.0f, 1.0f, 0.0f};
    verts[(5 * 4) + 3].normal = (vec3){0.0f, 1.0f, 0.0f};

    for (u32 i = 0; i < 24; ++i)
        verts[i].color = vec4_one();

    bcopy_memory(out_geometry.vertices, verts, out_geometry.vertex_element_size * out_geometry.vertex_count);

    for (u32 i = 0; i < 6; ++i)
    {
        u32 v_offset = i * 4;
        u32 i_offset = i * 6;
        ((u32*)out_geometry.indices)[i_offset + 0] = v_offset + 0;
        ((u32*)out_geometry.indices)[i_offset + 1] = v_offset + 1;
        ((u32*)out_geometry.indices)[i_offset + 2] = v_offset + 2;
        ((u32*)out_geometry.indices)[i_offset + 3] = v_offset + 0;
        ((u32*)out_geometry.indices)[i_offset + 4] = v_offset + 3;
        ((u32*)out_geometry.indices)[i_offset + 5] = v_offset + 1;
    }

    geometry_generate_tangents(out_geometry.vertex_count, out_geometry.vertices, out_geometry.index_count, out_geometry.indices);

    return out_geometry;
}

bgeometry geometry_generate_grid(grid_orientation orientation, u32 segment_count_dim_0, u32 segment_count_dim_1, f32 segment_scale, b8 use_third_axis, bname name)
{
    bgeometry out_geometry = {0};
    out_geometry.name = name;
    out_geometry.type = BGEOMETRY_TYPE_3D_STATIC;
    out_geometry.generation = INVALID_ID_U16;

    f32 max_0 = segment_count_dim_0 * segment_scale;
    f32 min_0 = -max_0;
    f32 max_1 = segment_count_dim_1 * segment_scale;
    f32 min_1 = -max_1;
    switch (orientation)
    {
    default:
    case GRID_ORIENTATION_XZ:
        out_geometry.extents.min.x = min_0;
        out_geometry.extents.max.x = max_0;
        out_geometry.extents.min.z = min_1;
        out_geometry.extents.max.z = max_1;
        break;
    case GRID_ORIENTATION_XY:
        out_geometry.extents.min.x = min_0;
        out_geometry.extents.max.x = max_0;
        out_geometry.extents.min.y = min_1;
        out_geometry.extents.max.y = max_1;
        break;
    case GRID_ORIENTATION_YZ:
        out_geometry.extents.min.y = min_0;
        out_geometry.extents.max.y = max_0;
        out_geometry.extents.min.z = min_1;
        out_geometry.extents.max.z = max_1;
        break;
    }
    // Always 0 since min/max of each axis are -/+ half of the size
    out_geometry.center = vec3_zero();
    out_geometry.vertex_element_size = sizeof(vertex_3d);
    // 2 verts per line, 1 line per tile in each direction, plus one in the middle for each direction. Adding 2 more for third axis
    out_geometry.vertex_count = ((segment_count_dim_0 * 2 + 1) * 2) + ((segment_count_dim_1 * 2 + 1) * 2) + 2;
    out_geometry.vertices = BALLOC_TYPE_CARRAY(vertex_3d, out_geometry.vertex_count);
    out_geometry.vertex_buffer_offset = INVALID_ID_U64;
    out_geometry.index_element_size = sizeof(u32);
    out_geometry.index_count = 0; // no indices
    out_geometry.indices = 0;
    out_geometry.index_buffer_offset = INVALID_ID_U64;

    // Generate vertex data

    // Grid line lengths are the amount of spaces in the opposite direction
    i32 line_length_0 = segment_count_dim_1 * segment_scale;
    i32 line_length_1 = segment_count_dim_0 * segment_scale;
    i32 line_length_2 = line_length_0 > line_length_1 ? line_length_0 : line_length_1;

    // f32 max_0 = segment_count_dim_0 * segment_scale;
    // f32 min_0 = -max_0;
    // f32 max_1 = segment_count_dim_1 * segment_scale;
    // f32 min_1 = -max_1;

    u32 element_index_0, element_index_1, element_index_2;

    switch (orientation)
    {
    default:
    case GRID_ORIENTATION_XZ:
        element_index_0 = 0; // x
        element_index_1 = 2; // z
        element_index_2 = 1; // y
        break;
    case GRID_ORIENTATION_XY:
        element_index_0 = 0; // x
        element_index_1 = 1; // y
        element_index_2 = 2; // z
        break;
    case GRID_ORIENTATION_YZ:
        element_index_0 = 1; // y
        element_index_1 = 2; // z
        element_index_2 = 0; // x
        break;
    }

    // First axis line
    ((color_vertex_3d*)out_geometry.vertices)[0].position.elements[element_index_0] = -line_length_1;
    ((color_vertex_3d*)out_geometry.vertices)[0].position.elements[element_index_1] = 0;
    ((color_vertex_3d*)out_geometry.vertices)[1].position.elements[element_index_0] = line_length_1;
    ((color_vertex_3d*)out_geometry.vertices)[1].position.elements[element_index_1] = 0;
    ((color_vertex_3d*)out_geometry.vertices)[0].color.elements[element_index_0] = 1.0f;
    ((color_vertex_3d*)out_geometry.vertices)[0].color.a = 1.0f;
    ((color_vertex_3d*)out_geometry.vertices)[1].color.elements[element_index_0] = 1.0f;
    ((color_vertex_3d*)out_geometry.vertices)[1].color.a = 1.0f;

    // Second axis line
    ((color_vertex_3d*)out_geometry.vertices)[2].position.elements[element_index_0] = 0;
    ((color_vertex_3d*)out_geometry.vertices)[2].position.elements[element_index_1] = -line_length_0;
    ((color_vertex_3d*)out_geometry.vertices)[3].position.elements[element_index_0] = 0;
    ((color_vertex_3d*)out_geometry.vertices)[3].position.elements[element_index_1] = line_length_0;
    ((color_vertex_3d*)out_geometry.vertices)[2].color.elements[element_index_1] = 1.0f;
    ((color_vertex_3d*)out_geometry.vertices)[2].color.a = 1.0f;
    ((color_vertex_3d*)out_geometry.vertices)[3].color.elements[element_index_1] = 1.0f;
    ((color_vertex_3d*)out_geometry.vertices)[3].color.a = 1.0f;

    if (use_third_axis)
    {
        // Third axis line
        ((color_vertex_3d*)out_geometry.vertices)[4].position.elements[element_index_0] = 0;
        ((color_vertex_3d*)out_geometry.vertices)[4].position.elements[element_index_2] = -line_length_2;
        ((color_vertex_3d*)out_geometry.vertices)[5].position.elements[element_index_0] = 0;
        ((color_vertex_3d*)out_geometry.vertices)[5].position.elements[element_index_2] = line_length_2;
        ((color_vertex_3d*)out_geometry.vertices)[4].color.elements[element_index_2] = 1.0f;
        ((color_vertex_3d*)out_geometry.vertices)[4].color.a = 1.0f;
        ((color_vertex_3d*)out_geometry.vertices)[5].color.elements[element_index_2] = 1.0f;
        ((color_vertex_3d*)out_geometry.vertices)[5].color.a = 1.0f;
    }

    vec4 alt_line_color = (vec4){1.0f, 1.0f, 1.0f, 0.5f};
    // calculate 4 lines at a time, 2 in each direction, min/max
    i32 j = 1;

    u32 start_index = use_third_axis ? 6 : 4;

    for (u32 i = start_index; i < out_geometry.vertex_count; i += 8)
    {
        // First line (max)
        ((color_vertex_3d*)out_geometry.vertices)[i + 0].position.elements[element_index_0] = j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 0].position.elements[element_index_1] = line_length_0;
        ((color_vertex_3d*)out_geometry.vertices)[i + 0].color = alt_line_color;
        ((color_vertex_3d*)out_geometry.vertices)[i + 1].position.elements[element_index_0] = j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 1].position.elements[element_index_1] = -line_length_0;
        ((color_vertex_3d*)out_geometry.vertices)[i + 1].color = alt_line_color;

        // Second line (min)
        ((color_vertex_3d*)out_geometry.vertices)[i + 2].position.elements[element_index_0] = -j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 2].position.elements[element_index_1] = line_length_0;
        ((color_vertex_3d*)out_geometry.vertices)[i + 2].color = alt_line_color;
        ((color_vertex_3d*)out_geometry.vertices)[i + 3].position.elements[element_index_0] = -j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 3].position.elements[element_index_1] = -line_length_0;
        ((color_vertex_3d*)out_geometry.vertices)[i + 3].color = alt_line_color;

        // Third line (max)
        ((color_vertex_3d*)out_geometry.vertices)[i + 4].position.elements[element_index_0] = -line_length_1;
        ((color_vertex_3d*)out_geometry.vertices)[i + 4].position.elements[element_index_1] = -j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 4].color = alt_line_color;
        ((color_vertex_3d*)out_geometry.vertices)[i + 5].position.elements[element_index_0] = line_length_1;
        ((color_vertex_3d*)out_geometry.vertices)[i + 5].position.elements[element_index_1] = -j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 5].color = alt_line_color;

        // Fourth line (min)
        ((color_vertex_3d*)out_geometry.vertices)[i + 6].position.elements[element_index_0] = -line_length_1;
        ((color_vertex_3d*)out_geometry.vertices)[i + 6].position.elements[element_index_1] = j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 6].color = alt_line_color;
        ((color_vertex_3d*)out_geometry.vertices)[i + 7].position.elements[element_index_0] = line_length_1;
        ((color_vertex_3d*)out_geometry.vertices)[i + 7].position.elements[element_index_1] = j * segment_scale;
        ((color_vertex_3d*)out_geometry.vertices)[i + 7].color = alt_line_color;

        j++;
    }

    return out_geometry;
}

void geometry_destroy(bgeometry* geometry)
{
    if (geometry)
    {
        if (geometry->vertices)
            bfree(geometry->vertices, geometry->vertex_count * geometry->vertex_element_size, MEMORY_TAG_ARRAY);
        if (geometry->indices)
            bfree(geometry->indices, geometry->index_count * geometry->index_element_size, MEMORY_TAG_ARRAY);

        bzero_memory(geometry, sizeof(bgeometry));

        // Setting this to invalidid effectively marks the geometry as "not setup"
        geometry->generation = INVALID_ID_U16;
        geometry->vertex_buffer_offset = INVALID_ID_U64;
        geometry->index_buffer_offset = INVALID_ID_U64;
    }
}
