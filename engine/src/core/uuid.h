#pragma once

#include "defines.h"

typedef struct uuid
{
    char value[37];
} uuid;

void uuid_seed(u64 seed);

uuid uuid_generate();
