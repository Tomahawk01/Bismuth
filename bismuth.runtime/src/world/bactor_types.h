#pragma once

#include "bresources/bresource_types.h"
#include "math/geometry.h"
#include "strings/bname.h"

typedef enum bactor_type
{
    BACTOR_TYPE_GROUP,
    BACTOR_TYPE_STATICMESH,
    BACTOR_TYPE_SKYBOX,
    BACTOR_TYPE_SKELETALMESH,
    BACTOR_TYPE_HEIGTMAP_TERRAIN
} bactor_type;

typedef struct bactor
{
    bname name;
    bactor_type type;
    b_handle xform;
} bactor;

typedef struct bactor_staticmesh
{
    bactor base;
    geometry g;
    bresource_material_instance material;
} bactor_staticmesh;
