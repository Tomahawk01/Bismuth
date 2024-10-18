#pragma once

#include "bresources/bresource_types.h"
#include "math/geometry.h"
#include "strings/bname.h"

/**
 * Actor is a in-world representation of something which exists in or can be spawned in the world. 
 * It may contain actor-components, which can be used to control how actors are rendered, move about in the world, sound, etc. 
 * Each actor component has reference to at least one resource, which is generally what gets rendered (i.e. a static mesh resource), but not always (i.e. sound effect)
 *
 * When used with a scene, these may be parented to one another via the scene's hierarchy view and xform graph, when attached to a scene node.
 */
typedef struct bactor
{
    bname name;
    b_handle xform;
} bactor;

typedef enum bactor_component_type
{
    BACTOR_COMPONENT_TYPE_UNKNOWN,
    BACTOR_COMPONENT_TYPE_STATICMESH,
    BACTOR_COMPONENT_TYPE_SKYBOX,
    BACTOR_COMPONENT_TYPE_SKELETALMESH,
    BACTOR_COMPONENT_TYPE_HEIGTMAP_TERRAIN,
    BACTOR_COMPONENT_TYPE_WATER_PLANE,
    BACTOR_COMPONENT_TYPE_DIRECTIONAL_LIGHT,
    BACTOR_COMPONENT_TYPE_POINT_LIGHT,
} bactor_component_type;

typedef struct bactor_component
{
    bname name;
    bactor_component_type type;
} bactor_component;

typedef struct bactor_component_staticmesh
{
    bactor_component base;
    geometry g;
    bresource_material_instance material;
} bactor_component_staticmesh;

BAPI bactor_component* bactor_component_get(const bactor* actor, bactor_component_type type, bname name);

BAPI b8 bactor_component_add(bactor* actor, bactor_component* component);
