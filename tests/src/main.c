#include "test_manager.h"
#include "memory/linear_allocator_tests.h"
#include "containers/hashtable_tests.h"

#include <core/logger.h>

int main()
{
    // Always initalize the test manager first
    test_manager_init();

    linear_allocator_register_tests();
    hashtable_register_tests();

    BDEBUG("Starting tests...");

    // Execute tests
    test_manager_run_tests();

    return 0;
}
