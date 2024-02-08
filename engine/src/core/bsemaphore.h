#ifndef _B_SEMAPHORE_H_
#define _B_SEMAPHORE_H_

#include "defines.h"

typedef struct bsemaphore
{
    void *internal_data;
} bsemaphore;

BAPI b8 bsemaphore_create(bsemaphore *out_semaphore, u32 max_count, u32 start_count);
BAPI void bsemaphore_destroy(bsemaphore *semaphore);

BAPI b8 bsemaphore_signal(bsemaphore *semaphore);
BAPI b8 bsemaphore_wait(bsemaphore *semaphore, u64 timeout_ms);

#endif
