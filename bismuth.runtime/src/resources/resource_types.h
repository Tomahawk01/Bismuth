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
