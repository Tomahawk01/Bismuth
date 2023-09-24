#pragma once

#include "defines.h"

typedef struct bmutex
{
    void *internal_data;
} bmutex;

b8 bmutex_create(bmutex* out_mutex);
void bmutex_destroy(bmutex* mutex);

b8 bmutex_lock(bmutex *mutex);
b8 bmutex_unlock(bmutex *mutex);
