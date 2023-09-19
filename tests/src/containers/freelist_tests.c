#include "freelist_tests.h"
#include "../test_manager.h"
#include "../expect.h"

#include <defines.h>
#include <containers/freelist.h>
#include <core/bmemory.h>

u8 freelist_should_create_and_destroy()
{
    // creating a small size list, which will trigger a warning
    BDEBUG("The following warning message is intentional");

    freelist list;

    // Get memory requirement
    u64 memory_requirement = 0;
    u64 total_size = 40;
    freelist_create(total_size, &memory_requirement, 0, 0);

    // Allocate and create freelist
    void* block = ballocate(memory_requirement, MEMORY_TAG_APPLICATION);
    freelist_create(total_size, &memory_requirement, block, &list);

    // Verify that memory was assigned
    expect_should_not_be(0, list.memory);
    // Verify that entire block is free
    u64 free_space = freelist_free_space(&list);
    expect_should_be(total_size, free_space);

    // Destroy and verify that memory was unassigned
    freelist_destroy(&list);
    expect_should_be(0, list.memory);
    bfree(block, memory_requirement, MEMORY_TAG_APPLICATION);

    return true;
}

u8 freelist_should_allocate_one_and_free_one()
{
    freelist list;

    // Get memory requirement
    u64 memory_requirement = 0;
    u64 total_size = 512;
    freelist_create(total_size, &memory_requirement, 0, 0);

    // Allocate and create freelist
    void* block = ballocate(memory_requirement, MEMORY_TAG_APPLICATION);
    freelist_create(total_size, &memory_requirement, block, &list);

    // Allocate some space
    u64 offset = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    b8 result = freelist_allocate_block(&list, 64, &offset);
    // Verify that result is true, offset should be set to 0
    expect_to_be_true(result);
    expect_should_be(0, offset);

    // Verify that correct amount of space is free
    u64 free_space = freelist_free_space(&list);
    expect_should_be(total_size - 64, free_space);

    // Now free the block
    result = freelist_free_block(&list, 64, offset);
    // Verify that result is true
    expect_to_be_true(result);

    // Verify that entire block is free
    free_space = freelist_free_space(&list);
    expect_should_be(total_size, free_space);

    // Destroy and verify that memory was unassigned
    freelist_destroy(&list);
    expect_should_be(0, list.memory);
    bfree(block, memory_requirement, MEMORY_TAG_APPLICATION);

    return true;
}

u8 freelist_should_allocate_one_and_free_multi()
{
    freelist list;

    // Get memory requirement
    u64 memory_requirement = 0;
    u64 total_size = 512;
    freelist_create(total_size, &memory_requirement, 0, 0);

    // Allocate and create freelist
    void* block = ballocate(memory_requirement, MEMORY_TAG_APPLICATION);
    freelist_create(total_size, &memory_requirement, block, &list);

    // Allocate some space
    u64 offset = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    b8 result = freelist_allocate_block(&list, 64, &offset);
    // Verify that result is true, offset should be set to 0
    expect_to_be_true(result);
    expect_should_be(0, offset);

    // Allocate some more space
    u64 offset2 = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    result = freelist_allocate_block(&list, 64, &offset2);
    // Verify that result is true, offset should be set to the size of the previous allocation
    expect_to_be_true(result);
    expect_should_be(64, offset2);

    // Allocate one more space
    u64 offset3 = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    result = freelist_allocate_block(&list, 64, &offset3);
    // Verify that result is true, offset should be set to the offset+size of the previous allocation
    expect_to_be_true(result);
    expect_should_be(128, offset3);

    // Verify that correct amount of space is free
    u64 free_space = freelist_free_space(&list);
    expect_should_be(total_size - 192, free_space);

    // Now free middle block
    result = freelist_free_block(&list, 64, offset2);
    // Verify that result is true
    expect_to_be_true(result);

    // Verify that correct amount is free
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 128, free_space);

    // Allocate some more space, this should fill the middle block back in
    u64 offset4 = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    result = freelist_allocate_block(&list, 64, &offset4);
    // Verify that result is true, offset should be set to the size of the previous allocation
    expect_to_be_true(result);
    expect_should_be(offset2, offset4);  // Offset should be the same as 2 since it occupies the same space

    // Verify that correct amount of space is free
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 192, free_space);

    // Free first block and verify space
    result = freelist_free_block(&list, 64, offset);
    expect_to_be_true(result);
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 128, free_space);

    // Free last block and verify space
    result = freelist_free_block(&list, 64, offset3);
    expect_to_be_true(result);
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 64, free_space);

    // Free middle block and verify space
    result = freelist_free_block(&list, 64, offset4);
    expect_to_be_true(result);
    free_space = freelist_free_space(&list);
    expect_should_be(total_size, free_space);

    // Destroy and verify that the memory was unassigned
    freelist_destroy(&list);
    expect_should_be(0, list.memory);
    bfree(block, memory_requirement, MEMORY_TAG_APPLICATION);

    return true;
}

u8 freelist_should_allocate_one_and_free_multi_varying_sizes()
{
    freelist list;

    // Get memory requirement
    u64 memory_requirement = 0;
    u64 total_size = 512;
    freelist_create(total_size, &memory_requirement, 0, 0);

    // Allocate and create freelist
    void* block = ballocate(memory_requirement, MEMORY_TAG_APPLICATION);
    freelist_create(total_size, &memory_requirement, block, &list);

    // Allocate some space
    u64 offset = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    b8 result = freelist_allocate_block(&list, 64, &offset);
    // Verify that result is true, offset should be set to 0
    expect_to_be_true(result);
    expect_should_be(0, offset);

    // Allocate some more space
    u64 offset2 = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    result = freelist_allocate_block(&list, 32, &offset2);
    // Verify that result is true, offset should be set to the size of the previous allocation
    expect_to_be_true(result);
    expect_should_be(64, offset2);

    // Allocate one more space
    u64 offset3 = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    result = freelist_allocate_block(&list, 64, &offset3);
    // Verify that result is true, offset should be set to offset+size of the previous allocation
    expect_to_be_true(result);
    expect_should_be(96, offset3);

    // Verify that correct amount of space is free
    u64 free_space = freelist_free_space(&list);
    expect_should_be(total_size - 160, free_space);

    // Now free the middle block
    result = freelist_free_block(&list, 32, offset2);
    // Verify that result is true
    expect_to_be_true(result);

    // Verify correct amount is free
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 128, free_space);

    // Allocate some more space, this time larger than the old middle block
    u64 offset4 = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    result = freelist_allocate_block(&list, 64, &offset4);
    // Verify that result is true, offset should be set to the size of the previous allocation
    expect_to_be_true(result);
    expect_should_be(160, offset4);  // Offset should be the end since it occupies more space than the old middle block

    // Verify that correct amount of space is free
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 192, free_space);

    // Free first block and verify space
    result = freelist_free_block(&list, 64, offset);
    expect_to_be_true(result);
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 128, free_space);

    // Free last block and verify space
    result = freelist_free_block(&list, 64, offset3);
    expect_to_be_true(result);
    free_space = freelist_free_space(&list);
    expect_should_be(total_size - 64, free_space);

    // Free middle (now end) block and verify space
    result = freelist_free_block(&list, 64, offset4);
    expect_to_be_true(result);
    free_space = freelist_free_space(&list);
    expect_should_be(total_size, free_space);

    // Destroy and verify that memory was unassigned
    freelist_destroy(&list);
    expect_should_be(0, list.memory);
    bfree(block, memory_requirement, MEMORY_TAG_APPLICATION);

    return true;
}

u8 freelist_should_allocate_to_full_and_fail_to_allocate_more()
{
    freelist list;

    // Get memory requirement
    u64 memory_requirement = 0;
    u64 total_size = 512;
    freelist_create(total_size, &memory_requirement, 0, 0);

    // Allocate and create freelist
    void* block = ballocate(memory_requirement, MEMORY_TAG_APPLICATION);
    freelist_create(total_size, &memory_requirement, block, &list);

    // Allocate all space
    u64 offset = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    b8 result = freelist_allocate_block(&list, 512, &offset);
    // Verify that result is true, offset should be set to 0
    expect_to_be_true(result);
    expect_should_be(0, offset);

    // Verify that correct amount of space is free
    u64 free_space = freelist_free_space(&list);
    expect_should_be(0, free_space);

    // Now try allocating some more
    u64 offset2 = INVALID_ID;  // Start with invalid id, which is a good default since it should never happen
    BDEBUG("The following warning message is intentional");
    result = freelist_allocate_block(&list, 64, &offset2);
    // Verify that result is false
    expect_to_be_false(result);


    // Verify that correct amount of space is free
    free_space = freelist_free_space(&list);
    expect_should_be(0, free_space);

    // Destroy and verify that memory was unassigned
    freelist_destroy(&list);
    expect_should_be(0, list.memory);
    bfree(block, memory_requirement, MEMORY_TAG_APPLICATION);

    return true;
}

void freelist_register_tests()
{
    test_manager_register_test(freelist_should_create_and_destroy, "Freelist should create and destroy");
    test_manager_register_test(freelist_should_allocate_one_and_free_one, "Freelist allocate and free one entry");
    test_manager_register_test(freelist_should_allocate_one_and_free_multi, "Freelist allocate and free multiple entries");
    test_manager_register_test(freelist_should_allocate_one_and_free_multi_varying_sizes, "Freelist allocate and free multiple entries of varying sizes");
    test_manager_register_test(freelist_should_allocate_to_full_and_fail_to_allocate_more, "Freelist allocate to full and fail when trying to allocate more");
}
