#include "bson_parser_tests.h"

#include <containers/darray.h>
#include <defines.h>
#include <parsers/bson_parser.h>
#include <platform/filesystem.h>
#include <strings/bstring.h>

#include "../expect.h"
#include "../test_manager.h"
#include "logger.h"

u8 bson_parser_should_create_and_destroy(void)
{
    bson_parser parser;
    bson_parser_create(&parser);

    expect_should_not_be(0, parser.tokens);
    expect_should_be(0, parser.position);
    expect_should_be(0, parser.file_content);

    bson_parser_destroy(&parser);

    expect_should_be(0, parser.tokens);
    expect_should_be(0, parser.position);
    expect_should_be(0, parser.file_content);

    return true;
}

u8 bson_parser_should_tokenize_file_content(void)
{
    // TODO: move to test asset folder
    char* full_file_path = "../bismuth.core.tests/src/parsers/test_scene2.bsn";
    file_handle f;
    if (!filesystem_open(full_file_path, FILE_MODE_READ, false, &f))
    {
        BERROR("text_loader_load - unable to open file for text reading: '%s'", full_file_path);
        return false;
    }

    u64 file_size = 0;
    if (!filesystem_size(&f, &file_size))
    {
        BERROR("Unable to text read file: %s", full_file_path);
        filesystem_close(&f);
        return false;
    }

    char* test_file_content = ballocate(sizeof(char) * file_size, MEMORY_TAG_ARRAY);
    u64 read_size = 0;
    if (!filesystem_read_all_text(&f, test_file_content, &read_size))
    {
        BERROR("Unable to text read file: %s", full_file_path);
        filesystem_close(&f);
        return false;
    }

    filesystem_close(&f);

    bson_parser parser;
    bson_parser_create(&parser);

    b8 tokenize_result = bson_parser_tokenize(&parser, test_file_content);
    // Start tokenizing
    expect_to_be_true(tokenize_result);

    u32 token_count = darray_length(parser.tokens);
    expect_should_not_be(0, token_count);

    // Parse the tokens
    bson_tree tree;
    b8 parse_result = bson_parser_parse(&parser, &tree);
    expect_to_be_true(parse_result);

    bson_parser_destroy(&parser);

    const char* str = bson_tree_to_string(&tree);
    BINFO(str);

    return true;
}

void bson_parser_register_tests(void)
{
    test_manager_register_test(bson_parser_should_create_and_destroy, "BSON parser should create and destroy");
    test_manager_register_test(bson_parser_should_tokenize_file_content, "BSON parser should tokenize file content");
}
