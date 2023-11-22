#pragma once

#include "defines.h"

typedef struct identifier
{
    // Actual internal identifier
    u64 uniqueid;
} identifier;

BAPI identifier identifier_create(void);
BAPI identifier identifier_from_u64(u64 uniqueid);
