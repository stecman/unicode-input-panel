#pragma once

#include <stdint.h>
#include <string>

const uint32_t kInvalidEncoding = 0xFFFFFFFF;

/**
 * Reallocate a container to exactly fit its contents
 */
template<typename C> void shrinkContainer(C &container) {
    if (container.size() != container.capacity()) {
        C tmp = container;
        swap(container, tmp);
    }
}

/**
 * Encode a Unicode codepoint as UTF-8
 * 
 * Returns a null-terminated UTF-8 sequence representing the codepoint,
 * or nullptr if the passed value cannot be encoded as UTF-8.
 * 
 * The returned pointer is valid until this function is called again.
 */
const char* codepoint_to_utf8(uint32_t codepoint);

/**
 * Convert a single UTF-8 sequence to codepoint
 * 
 * Returns kInvalidEncoding if the sequence is not valid UTF-8
 */
uint32_t utf8_to_codepoint(uint8_t encoded[4]);

/**
 * Test if a byte is a UTF-8 sequence continuation ("0b10...")
 */
inline bool is_utf8_continuation(uint8_t byte)
{
    return (byte & 0xC0) == 0x80;
}