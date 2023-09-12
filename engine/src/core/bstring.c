#include "core/bstring.h"
#include "core/bmemory.h"

#include <string.h>

u64 string_length(const char* str)
{
    return strlen(str);
}

char* string_duplicate(const char* str)
{
    u64 length = string_length(str);
    char* copy = ballocate(length + 1, MEMORY_TAG_STRING);
    bcopy_memory(copy, str, length + 1);
    return copy;
}
