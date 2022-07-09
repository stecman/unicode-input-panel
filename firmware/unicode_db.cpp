#include "unicode_db.hh"

#include <cstring>

// Private reference to embedded unicode metadata
// These variables are declared in scripts/build-metadata.py
extern "C" {
    #include "unicode_data.h"

    extern char* uc_name_buffer;
    extern const uint16_t uc_name_buffer_len;

    extern const uint32_t uc_blocks_len;
    extern const struct UCBlockRange uc_blocks[];

    extern const uint32_t uc_name_indices_len;
    extern const struct UCIndex uc_name_indices[];

    extern const uint32_t uc_name_prefixes_len;
    extern const char* uc_name_prefixes[];

    extern const uint32_t uc_names_len;
    extern const struct UCPrefixedString uc_names[];
}

static const UCPrefixedString* find_name(uint32_t codepoint)
{
    const uint16_t end = uc_name_indices_len - 1;

    int32_t left = 0;
    int32_t right = end;

    while (left <= right)
    {
        const uint32_t mid = left + (right - left)/2;

        const UCIndex &current = uc_name_indices[mid];

        // Calculate the length of the current range of codepoint names
        uint16_t length;
        if (mid == end) {
            length = uc_names_len - current.index;
        } else {
            const UCIndex &next = uc_name_indices[mid + 1];
            length = next.index - current.index;
        }

        // Check if we're in the current range
        if (codepoint >= current.codepoint && codepoint < (current.codepoint + length)) {
            const uint16_t offset = codepoint - current.codepoint;
            return uc_names + current.index + offset;
        }

        if (current.codepoint < codepoint) {
            // Ignore left half
            left = mid + 1;
        } else {
            // Ignore right half
            right = mid - 1;
        }
    }

    // Failed to find a name for the codepoint
    return NULL;
}

/**
 * Find the block name for a codepoint by binary search, or return NULL
 */
const char* uc_get_block_name(uint32_t codepoint)
{
    int32_t left = 0;
    int32_t right = uc_blocks_len - 1;

    while (left <= right)
    {
        const uint32_t mid = left + (right - left)/2;

        // Test if codepoint is at the mid-point
        const UCBlockRange &range = uc_blocks[mid];

        if (codepoint >= range.start && codepoint <= range.end) {
            return range.name;
        }

        if (left == right) {
            // Reached last option and it didn't match
            break;
        }

        if (range.start < codepoint) {
            // Ignore left half
            left = mid + 1;
        } else {
            // Ignore right half
            right = mid - 1;
        }
    }

    // Failed to find a block containing the codepoint
    return NULL;
}

const char* uc_get_codepoint_name(uint32_t codepoint)
{
    const UCPrefixedString* found = find_name(codepoint);
    if (found == NULL) {
        // Not a named codepoint
        return NULL;
    }

    if (found->prefix_index == 0xFFFF) {
        // Plain name without prefix
        return found->name;
    }

    uc_name_buffer[0] = '\0';
    std::strcat(uc_name_buffer, uc_name_prefixes[found->prefix_index]);
    std::strcat(uc_name_buffer, found->name);

    return uc_name_buffer;
}
