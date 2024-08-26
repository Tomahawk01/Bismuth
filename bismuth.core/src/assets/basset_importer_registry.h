#pragma once

#include "assets/basset_types.h"
#include "defines.h"

BAPI b8 basset_importer_registry_initialize(void);
BAPI void basset_importer_registry_shutdown(void);
BAPI b8 basset_importer_registry_register(basset_type type, const char* source_type, basset_importer importer);

BAPI const basset_importer* basset_importer_registry_get_for_source_type(basset_type type, const char* source_type);
