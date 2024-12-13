#include "../expect.h"
#include "../test_manager.h"

#include <defines.h>
#include <memory/bmemory.h>
#include <strings/bstring.h>

static u8 bstr_ncmp_tests(void)
{
    const char* str_a = "texture";
    const char* str_b = "text";
    // set max length of shorter string
    {
        // Same strings
        i64 result = bstr_ncmp(str_a, str_a, 4);
        expect_should_be(0, result);
        // First string longer than the second
        result = bstr_ncmp(str_a, str_b, 4);
        expect_should_be(0, result);
        // Second string longer than the first
        result = bstr_ncmp(str_b, str_a, 4);
        expect_should_be(0, result);
    }
    // u32 max length
    {
        // Same strings
        i64 result = bstr_ncmp(str_a, str_a, U32_MAX);
        expect_should_be(0, result);
        // First string longer than the second
        result = bstr_ncmp(str_a, str_b, U32_MAX);
        expect_should_be(117, result);
        // Second string longer than the first
        result = bstr_ncmp(str_b, str_a, U32_MAX);
        expect_should_be(-117, result);
    }
    
    return true;
}

void string_register_tests(void)
{
    test_manager_register_test(bstr_ncmp_tests, "All bstr_ncmp tests pass");
}
