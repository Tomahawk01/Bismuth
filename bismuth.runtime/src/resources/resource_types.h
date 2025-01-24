#pragma once

#include "identifiers/identifier.h"
#include "bresources/bresource_types.h"
#include "math/math_types.h"

#include <core_render_types.h>

#define TERRAIN_MAX_MATERIAL_COUNT 4

// Pre-defined resource types
typedef enum resource_type
{
    RESOURCE_TYPE_TEXT,
    RESOURCE_TYPE_BINARY,
    RESOURCE_TYPE_IMAGE,
    RESOURCE_TYPE_MATERIAL,
    RESOURCE_TYPE_SHADER,
    RESOURCE_TYPE_MESH,
    RESOURCE_TYPE_BITMAP_FONT,
    RESOURCE_TYPE_SYSTEM_FONT,
    RESOURCE_TYPE_scene,
    RESOURCE_TYPE_TERRAIN,
    RESOURCE_TYPE_AUDIO,
    RESOURCE_TYPE_CUSTOM
} resource_type;

// Magic number indicating the file is a bismuth binary file
#define RESOURCE_MAGIC 0xdeadbeef

typedef struct resource_header
{
    u32 magic_number;
    u8 resource_type;
    u8 version;
    u16 reserved;
} resource_header;

typedef struct resource
{
    u32 loader_id;
    const char* name;
    char* full_path;
    u64 data_size;
    void* data;
} resource;

typedef enum scene_node_attachment_type
{
    SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN,
    SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH,
    SCENE_NODE_ATTACHMENT_TYPE_TERRAIN,
    SCENE_NODE_ATTACHMENT_TYPE_SKYBOX,
    SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT,
    SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT,
    SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE
} scene_node_attachment_type;

// Static mesh attachment
typedef struct scene_node_attachment_static_mesh
{
    char* resource_name;
} scene_node_attachment_static_mesh;

// Terrain attachment
typedef struct scene_node_attachment_terrain
{
    char* name;
    char* resource_name;
} scene_node_attachment_terrain;

// Skybox attachment
typedef struct scene_node_attachment_skybox
{
    char* cubemap_name;
} scene_node_attachment_skybox;

// Directional light attachment
typedef struct scene_node_attachment_directional_light
{
    vec4 color;
    vec4 direction;
    f32 shadow_distance;
    f32 shadow_fade_distance;
    f32 shadow_split_mult;
} scene_node_attachment_directional_light;

typedef struct scene_node_attachment_point_light
{
    vec4 color;
    vec4 position;
    f32 constant_f;
    f32 linear;
    f32 quadratic;
} scene_node_attachment_point_light;

// Skybox attachment
typedef struct scene_node_attachment_water_plane
{
    u32 reserved;
} scene_node_attachment_water_plane;

typedef struct scene_node_attachment_config
{
    scene_node_attachment_type type;
    void* attachment_data;
} scene_node_attachment_config;

typedef struct scene_xform_config
{
    vec3 position;
    quat rotation;
    vec3 scale;
} scene_xform_config;

typedef struct scene_node_config
{
    char* name;

    // Pointer to a config if one exists, otherwise 0
    scene_xform_config* xform;
    // darray
    scene_node_attachment_config* attachments;
    // darray
    struct scene_node_config* children;
} scene_node_config;

typedef struct scene_config
{
    u32 version;
    char* name;
    char* description;
    char* resource_name;
    char* resource_full_path;

    // darray
    scene_node_config* nodes;
} scene_config;
