#include "bstring_id.h"

#include "containers/u64_bst.h"
#include "debug/bassert.h"
#include "bstring.h"
#include "logger.h"
#include "utils/crc64.h"

// Global lookup table for saved strings
static bt_node* bstring_id_lookup = 0;

bstring_id bstring_id_create(const char* str)
{
    if (!str || string_length(str) == 0)
    {
        BERROR("bstring_id_create requires a valid pointer to a string and the string must have a nonzero length");
        return INVALID_BSTRING_ID;
    }

    // Take a copy in case it was dynamically allocated and might later be freed
    // This copy of the original string is stored for reference and can later be looked up
    char* copy = string_duplicate(str);

    // Hash the copy of the string
    bstring_id new_string_id = crc64(0, (const u8*)copy, string_length(copy));
    // NOTE: A hash of 0 is never allowed
    BASSERT_MSG(new_string_id != 0, string_format("bstring_id_create - provided string '%s' hashed to 0, an invalid value. Please change the string to something else to avoid this", str));

    // Register in a global lookup table if not already there
    const bt_node* entry = u64_bst_find(bstring_id_lookup, new_string_id);
    if (!entry)
    {
        bt_node_value value;
        value.str = copy;
        bt_node* inserted = u64_bst_insert(bstring_id_lookup, new_string_id, value);
        if (!inserted)
        {
            BERROR("Failed to save bstring_id string '%s' to global lookup table.");
        }
        else if (!bstring_id_lookup)
        {
            bstring_id_lookup = inserted;
        }
    }

    return new_string_id;
}

const char* bstring_id_string_get(bstring_id stringid)
{
    const bt_node* entry = u64_bst_find(bstring_id_lookup, stringid);
    if (entry)
    {
        // NOTE: For now, just return the existing pointer to the string
        // If this ever becomes a problem, return a copy instead
        return entry->value.str;
    }

    return 0;
}
