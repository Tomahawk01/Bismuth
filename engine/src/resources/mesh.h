#pragma once

#include "resource_types.h"

BAPI b8 mesh_load_from_resource(const char* resource_name, mesh* out_mesh);

BAPI void mesh_unload(mesh* m);
