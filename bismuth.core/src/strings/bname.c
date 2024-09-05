#include "bname.h"

#include "containers/u64_bst.h"
#include "debug/bassert.h"
#include "logger.h"
#include "strings/bstring.h"
#include "utils/crc64.h"

// Global lookup table for saved names
static bt_node* string_lookup = 0;

bname bname_create(const char* str)
{
    // Take a copy of the string to hash
    char* copy = string_duplicate(str);
    // Convert it to lowercase before hashing
    string_to_lower(copy);

    // Hash the lowercase string
    bname name = crc64(0, (const u8*)copy, string_length(copy));
    // NOTE: A hash of 0 is never allowed
    BASSERT_MSG(name != 0, string_format("bname_create - provided string '%s' hashed to 0, an invalid value. Please change the string to something else to avoid this", str));

    // Dispose of the lowercase string
    string_free(copy);

    // Register in a global lookup table if not already there
    const bt_node* entry = u64_bst_find(string_lookup, name);
    if (!entry)
    {
        // Take a copy in case it was dynamically allocated and might later be freed.
        // Storing a copy of the *original* string for reference, even though this is _not_ what is used for lookup
        bt_node_value value;
        value.str = string_duplicate(str);
        bt_node* inserted = u64_bst_insert(string_lookup, name, value);
        if (!inserted)
        {
            BERROR("Failed to save bname string '%s' to global lookup table");
        }
        else if (!string_lookup)
        {
            string_lookup = inserted;
        }
    }
    return name;
}

const char* bname_string_get(bname name)
{
    const bt_node* entry = u64_bst_find(string_lookup, name);
    if (entry)
    {
        // NOTE: For now, just return the existing pointer to the string.
        // If this ever becomes a problem, return a copy instead
        return entry->value.str;
    }

    return 0;
}
