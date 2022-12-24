#pragma once

#include "ui/font.hh"

/**
 * Glyph rendering with fallback for non-renderable codepoints
 * (eg. valid but missing glyphs, control characters, invalid codepoints)
 */
class GlyphDisplay
{
public:
    /**
     * @param y_offset - Offset the glyph from the default center of the screen
     */
    GlyphDisplay(FontStore& fontstore, int y_offset = 0);

    /**
     * Draw the passed codepoint centered on screen
     * 
     * @param is_valid - Hint if the codepoint is technically valid or not.
     *                   If no font has a glyph for the codepoint, this controls
     *                   if draw is treated as a missing glyph or an invalid value.
     */
    void draw(uint32_t codepoint, bool is_valid);

    /**
     * Clear the last drawn codepoint or fallback
     */
    void clear();

private:

    /**
     * Attempt to find a font and draw a glyph
     * Returns true if the glyph was successfully drawn.
     */
    bool drawGlyph(uint32_t codepoint);

private:

    enum Result {
        kResult_None,
        kResult_DrewGlyph,
        kResult_ControlChar,
        kResult_MissingGlyph,
        kResult_InvalidCodepoint,
    };

    // Vertical offset from screen center
    int m_y_offset;

    Result m_last_result;

    // Last drawn regions for quick blanking
    UIRect m_last_draw;
    UIRect m_last_fallback_draw;

    FontStore& m_fontstore;
};