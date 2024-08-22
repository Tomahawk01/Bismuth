#pragma once

#include "containers/queue.h"
#include "threads/bmutex.h"
#include "threads/bsemaphore.h"
#include "threads/bthread.h"
#include "defines.h"

typedef struct worker_thread
{
    bthread thread;
    queue work_queue;
    bmutex queue_mutex;
} worker_thread;

BAPI b8 worker_thread_create(worker_thread* out_thread);
BAPI void worker_thread_destroy(worker_thread* thread);

BAPI b8 worker_thread_add(worker_thread* thread, pfn_thread_start work_fn, void* params);
BAPI b8 worker_thread_start(worker_thread* thread);
BAPI b8 worker_thread_wait(worker_thread* thread);
