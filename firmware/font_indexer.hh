#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <limits>
#include <vector>

#include <stdint.h>


struct CodepointRange {
    uint32_t start;
    uint32_t end;
    uint8_t id;

    static bool compare_starts(const CodepointRange &a, const CodepointRange &b)
    {
        return a.start < b.start;
    }

    CodepointRange()
        : start(std::numeric_limits<uint32_t>::max()),
          end(std::numeric_limits<uint32_t>::max()),
          id(0) {}

    CodepointRange(const uint32_t &_start, const uint32_t &_end, const uint8_t &_id)
        : start(_start),
          end(_end),
          id(_id) {}
};

/**
 * Sparse map of which codepoints come from which font
 *
 * This is designed for use on a microcontroller where there's not enough memory
 * to simply use a std::map of codepoint to font/id. This still uses a lot of
 * memory for a microcontroller, but it's ~50KB instead of ~250KB for the full
 * set of Noto Regular fonts (228 fonts with 51511 unique codepoints).
 *
 * Currently uses heap allocation to store codepoint range objects.
 */
class FontIndexer
{
public:
    enum ErrorCode {
        kCodepointNotFound = std::numeric_limits<uint16_t>::max(),
    };

    /**
     * Associates codepoints in the passed font with the passed ID
     *
     * IDs are 8-bit to minimise the storage requirement.
     *
     * Note the order fonts are indexed matters: a codepoint is associated with
     * the first font that contains it. This association will stick unless a
     * font has an longer sequence of codepoints overlapping an existing one.
     */
    void indexFace(const uint8_t id, FT_Face face);

    /**
     * Compress ranges to save memory
     * 
     * Merges unclaimed ranges (gaps) into neighbouring ranges, allowing smaller
     * ranges of each font to be collapsed together. This significantly reduces
     * memory consumption, with the trade off that false-positve matches will be
     * found and that font must be loaded to know if a glyph actually exists.
     * 
     * All calls to indexFace() must be made before calling compressRanges(). Once
     * compressed, new fonts will be unable to merge as all codepoints will appear
     * to be claimed by other fonts.
     */
    void compressRanges();

    /**
     * Find the ID associated with the passed codepoint
     * Returns FontIndexer::kCodepointNotFound if not in the index
     */
    uint16_t find(const uint32_t codepoint);

    /**
     * Count the number of unique codepoints in the index
     */
    uint32_t countCodepoints();

    inline const std::vector<CodepointRange>& ranges()
    {
        return m_ranges;
    }

private:
    /**
     * Merge a list of codepoint ranges into the main index
     */
    void mergeRanges(std::vector<CodepointRange> &incoming_ranges);

    void orderRanges();

    // Cache of actual codepoint code to use after compressRanges is called
    // Zero indicates codepoints must to be counted (no cached value)
    uint32_t m_cached_count = 0;

    std::vector<CodepointRange> m_ranges;
};