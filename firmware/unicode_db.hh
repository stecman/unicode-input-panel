#pragma once

#include <stdint.h>

const char* uc_get_block_name(uint32_t codepoint);

const char* uc_get_codepoint_name(uint32_t codepoint);

inline bool is_control_char(uint32_t codepoint)
{
    return (codepoint <= 0x001F) || (codepoint >= 0x007F && codepoint <= 0x009F);
}