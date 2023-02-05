#include "util.hh"

const char* codepoint_to_utf8(uint32_t codepoint)
{
    static char output[5];

    uint8_t stack[4];
    uint32_t index = 0;

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

    const uint32_t numbytes = index;
    for (uint32_t i = 0; i < numbytes; i++) {
        output[i] = stack[--index];
    }

    output[numbytes] = '\0';

    return (char*) &output;
}

uint32_t utf8_to_codepoint(uint8_t seq[4])
{
    uint32_t codepoint = 0;

    if (seq[0] < 0x80) {
        // 1 byte sequence
        return seq[0];
    }

    uint8_t header = seq[0] >> 3;
    if (header == 0b11110) {
        // 4 byte sequence
        if (!is_utf8_continuation(seq[1]) ||
            !is_utf8_continuation(seq[2]) ||
            !is_utf8_continuation(seq[3])) {
            return kInvalidEncoding;
        }

        codepoint = seq[0] & 0x07;
        codepoint <<= 6;
        codepoint |= seq[1] & 0x3F;
        codepoint <<= 6;
        codepoint |= seq[2] & 0x3F;
        codepoint <<= 6;
        codepoint |= seq[3] & 0x3F;

        return codepoint;
    }

    header >>= 1;
    if (header == 0b1110) {
        // 3 byte sequence
        if (!is_utf8_continuation(seq[1]) ||
            !is_utf8_continuation(seq[2])) {
            return kInvalidEncoding;
        }
        
        codepoint = seq[0] & 0x0F;
        codepoint <<= 6;
        codepoint |= seq[1] & 0x3F;
        codepoint <<= 6;
        codepoint |= seq[2] & 0x3F;

        return codepoint;
    }
    
    header >>= 1;
    if (header == 0b110) {
        // 2 byte sequence
        if (!is_utf8_continuation(seq[1])) {
            return kInvalidEncoding;
        }

        codepoint = seq[0] & 0x1F;
        codepoint <<= 6;
        codepoint |= seq[1] & 0x3F;

        return codepoint;
    }

    return kInvalidEncoding;
}
