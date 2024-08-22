#pragma once

#include "defines.h"

typedef struct uuid
{
    char value[37];
} uuid;

BAPI void uuid_seed(u64 seed);

BAPI uuid uuid_generate(void);
