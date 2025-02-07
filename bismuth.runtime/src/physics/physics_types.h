#pragma once

#include "identifiers/bhandle.h"
#include "math/math_types.h"
#include "strings/bname.h"

struct bphysics_body;

typedef struct bphysics_world
{
    bname name;
    vec3 gravity;
    
    // darray of handles to physics bodies
    bhandle* bodies;
} bphysics_world;

typedef struct bphysics_system_config
{
    u32 steps_per_frame;
} bphysics_system_config;
