#include "util.hh"

const char* codepoint_to_utf8(uint32_t codepoint)
{
    static char output[5];

    uint8_t stack[4];
    uint index = 0;

    if (codepoint <= 0x7F) {
        stack[index++] = codepoint;
    } else if (codepoint <= 0x7FF) {
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0xC0 | (codepoint & 0x1F);
    } else if (codepoint <= 0xFFFF) {
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0xE0 | (codepoint & 0x0F);
    } else if (codepoint <= 0x1FFFFF) {
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0xF0 | (codepoint & 0x07);
    } else {
        return nullptr;
    }

    const uint numbytes = index;
    for (uint i = 0; i < numbytes; i++) {
        output[i] = stack[--index];
    }

    output[numbytes] = '\0';

    return (char*) &output;
}