#pragma once

#include "containers/queue.h"
#include "defines.h"

typedef struct bthread
{
    void *internal_data;
    u64 thread_id;
    queue work_queue;
} bthread;

typedef u32 (*pfn_thread_start)(void *);

BAPI b8 bthread_create(pfn_thread_start start_function_ptr, void *params, b8 auto_detach, bthread *out_thread);
BAPI void bthread_destroy(bthread *thread);

BAPI void bthread_detach(bthread *thread);

BAPI void bthread_cancel(bthread *thread);

BAPI b8 bthread_wait(bthread *thread);
BAPI b8 bthread_wait_timeout(bthread *thread, u64 wait_ms);

BAPI b8 bthread_is_active(bthread *thread);

BAPI void bthread_sleep(bthread *thread, u64 ms);

BAPI u64 platform_current_thread_id(void);
