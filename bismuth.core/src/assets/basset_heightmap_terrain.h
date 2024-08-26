#pragma once

#include "defines.h"
#include "basset_types.h"
#include "math/math_types.h"

struct basset;

/** @brief Represents an asset used to create a heightmap terrain */
typedef struct basset_heightmap_terrain
{
    /** @brief The base asset data */
    basset base;
} basset_heightmap_terrain;
