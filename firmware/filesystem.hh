#pragma once

#include "ft2build.h"
#include FT_FREETYPE_H

#include <algorithm>
#include <functional>
#include <string>

#include <stdint.h>

namespace fs {

/**
 * Prepare the filesystem for access
 * Returns non-zero if the operation failed
 */
int mount();

/**
 * Load the font at the given path
 *
 * Any allocations made for file handling are freed automatically
 * when FT_Face_Done is called.
 */
FT_Error load_face(const char* path, FT_Library library, FT_Face* face);

/**
 * Check if a path is a directory on disk
 */
bool is_dir(const char* path);

/**
 * Visit each file in the passed directory
 */
void walkdir(const char* dirpath,
             const std::function<void(const char* abspath, uint8_t progress)> &callback);

/**
 * Calculate percentage (value/max) as a full 8-bit range, where 0x0=0% and 0xFF=100%
 * Uses fixed point math as the Pico lacks a FPU
 */
inline uint8_t fp_progress(uint32_t value, uint32_t max)
{
    return std::clamp(((value << 8) / max), (uint32_t) 0, (uint32_t) 255);
}

inline bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

}; // namespace fs
