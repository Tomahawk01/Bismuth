#pragma once

#include "defines.h"

typedef b8 (*pfn_job_start)(void*, void*);

typedef void (*pfn_job_on_complete)(void*);

struct frame_data;

typedef enum job_type
{
    JOB_TYPE_GENERAL = 0x02,
    JOB_TYPE_RESOURCE_LOAD = 0x04,
    JOB_TYPE_GPU_RESOURCE = 0x08,
} job_type;

typedef enum job_priority
{
    JOB_PRIORITY_LOW,
    JOB_PRIORITY_NORMAL,
    JOB_PRIORITY_HIGH
} job_priority;

typedef struct job_info
{
    job_type type;

    job_priority priority;

    pfn_job_start entry_point;

    pfn_job_on_complete on_success;

    pfn_job_on_complete on_fail;

    void* param_data;

    u32 param_data_size;

    void* result_data;

    u32 result_data_size;
} job_info;

typedef struct job_system_config
{
    u8 max_job_thread_count;
    u32* type_masks;
} job_system_config;

b8 job_system_initialize(u64* job_system_memory_requirement, void* state, void* config);
void job_system_shutdown(void* state);

b8 job_system_update(void* state, struct frame_data* p_frame_data);

BAPI void job_system_submit(job_info info);

BAPI job_info job_create(pfn_job_start entry_point, pfn_job_on_complete on_success, pfn_job_on_complete on_fail, void* param_data, u32 param_data_size, u32 result_data_size);

BAPI job_info job_create_type(pfn_job_start entry_point, pfn_job_on_complete on_success, pfn_job_on_complete on_fail, void* param_data, u32 param_data_size, u32 result_data_size, job_type type);

BAPI job_info job_create_priority(pfn_job_start entry_point, pfn_job_on_complete on_success, pfn_job_on_complete on_fail, void* param_data, u32 param_data_size, u32 result_data_size, job_type type, job_priority priority);
