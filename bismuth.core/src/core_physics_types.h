#pragma once

typedef enum bphysics_body_type
{
    BPHYSICS_BODY_TYPE_STATIC,
    BPHYSICS_BODY_TYPE_DYNAMIC,
} bphysics_body_type;

typedef enum bphysics_shape_type
{
    BPHYSICS_SHAPE_TYPE_SPHERE,
    BPHYSICS_SHAPE_TYPE_RECTANGLE,
    /* BPHYSICS_SHAPE_TYPE_CAPSULE, */
    BPHYSICS_SHAPE_TYPE_MESH,
} bphysics_shape_type;
