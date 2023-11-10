#pragma once

#include "resource_types.h"

//BAPI b8 mesh_load_from_resource(const char* resource_name, mesh* out_mesh);

BAPI b8 mesh_create(mesh_config config, mesh* out_mesh);

BAPI b8 mesh_initialize(mesh* m);

BAPI b8 mesh_load(mesh* m);

BAPI b8 mesh_unload(mesh* m);

BAPI b8 mesh_destroy(mesh* m);
