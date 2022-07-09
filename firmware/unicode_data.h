#pragma once

#include <stdint.h>

//
// Structures for use by generated code
// See scripts/build-metadata.py
//

// Named range of codepoints
struct UCBlockRange {
    uint32_t start;
    uint32_t end;
    const char* name;
};

// Association of a codepoint to an index in an array
// With an array of these structs, the number of valid values can be determined
// by looking at the next item in the array: (next.index - current.index).
struct UCIndex {
    uint32_t codepoint;
    uint16_t index;
};

// An index to a string prefix, and the remainder of that string
// The prefix is optional (should be 0xFFFF when unused).
struct UCPrefixedString {
    uint16_t prefix_index;
    const char* name;
};