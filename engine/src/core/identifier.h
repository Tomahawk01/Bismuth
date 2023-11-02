#pragma once

#include "defines.h"

BAPI u32 identifier_aquire_new_id(void* owner);
BAPI void identifier_release_id(u32 id);
