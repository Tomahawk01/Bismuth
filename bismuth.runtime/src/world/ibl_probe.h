#pragma once

#include "bresources/bresource_types.h"
#include "math/math_types.h"
#include "strings/bname.h"

typedef struct ibl_probe
{
    bname cubemap_name;
    bresource_texture* ibl_cube_texture;
    vec3 position;
} ibl_probe;

BAPI b8 ibl_probe_create(bname cubemap_name, vec3 position, ibl_probe* out_probe);
BAPI void ibl_probe_destroy(ibl_probe* probe);

BAPI b8 ibl_probe_load(ibl_probe* probe);
BAPI void ibl_probe_unload(ibl_probe* probe);
