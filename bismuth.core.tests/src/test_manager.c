#include "test_manager.h"

#include <containers/darray.h>
#include <logger.h>
#include <strings/bstring.h>
#include <time/bclock.h>

typedef struct test_entry
{
    PFN_test func;
    char* desc;
} test_entry;

static test_entry* tests;

void test_manager_init(void)
{
    tests = darray_create(test_entry);
}

void test_manager_register_test(u8 (*PFN_test)(void), char* desc)
{
    test_entry e;
    e.func = PFN_test;
    e.desc = desc;
    darray_push(tests, e);
}

void test_manager_run_tests(void)
{
    u32 passed = 0;
    u32 failed = 0;
    u32 skipped = 0;

    u32 count = darray_length(tests);

    bclock total_time;
    bclock_start(&total_time);

    for (u32 i = 0; i < count; ++i)
    {
        bclock test_time;
        bclock_start(&test_time);
        u8 result = tests[i].func();
        bclock_update(&test_time);

        if (result == true)
        {
            ++passed;
        }
        else if (result == BYPASS)
        {
            BWARN("[SKIPPED]: %s", tests[i].desc);
            ++skipped;
        }
        else
        {
            BERROR("[FAILED]: %s", tests[i].desc);
            ++failed;
        }
        char status[20];
        string_format_unsafe(status, failed ? "*** %d FAILED ***" : "SUCCESS", failed);
        bclock_update(&total_time);
        BINFO("Executed %d of %d (skipped %d) %s (%.6f sec / %.6f sec total", i + 1, count, skipped, status, test_time.elapsed, total_time.elapsed);
    }

    bclock_stop(&total_time);

    BINFO("Results: %d passed, %d failed, %d skipped", passed, failed, skipped);
}
