#pragma once

#include "defines.h"
#include "core/bmemory.h"
#include "resources/resource_types.h"

struct resource_loader;

BAPI b8 resource_unload(struct resource_loader* self, resource* resource, memory_tag tag);
