#pragma once

#include <stdint.h>
#include <string>

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