#pragma once

#include "defines.h"

typedef struct bmutex
{
    void *internal_data;
} bmutex;

BAPI b8 bmutex_create(bmutex* out_mutex);
BAPI void bmutex_destroy(bmutex* mutex);

BAPI b8 bmutex_lock(bmutex *mutex);
BAPI b8 bmutex_unlock(bmutex *mutex);
