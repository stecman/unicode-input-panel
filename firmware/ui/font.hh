#pragma once

#include "font_indexer.hh"
#include "ui/common.hh"
#include "util.hh"

// FreeType
#include "ft2build.h"
#include FT_CACHE_H
#include FT_FREETYPE_H

// C++
#include <string>
#include <vector>

/**
 * Rendering state for drawing text in the UI
 */
class UIFontPen {
public:

    enum RenderMode {
        // (Default) Render completely in memory, then write to screen with a single DMA request
        // This can use a lot of memory, so you may want to use the other modes for large font sizes.
        kMode_CanvasBuffer,

        // Render using existing single line buffers, writing each line to screen with a DMA request
        kMode_LineBuffer,

        // Render only the required pixels directly to screen, without storing in memory first
        // This is slower, but allows for a crude form of blending with existing screen contents.
        // Glyphs are rendered one by one from bottom to top.
        kMode_DirectToScreen,
    };

    /**
     * Clear any currently loaded font shared between UIFontPen instances
     *
     * This must be called manually after UIFontPen instances have been
     * destructed to release the last used memory font.
     */
    static void unload_shared();

    UIFontPen(const uint8_t* fontdata, size_t length, FT_Library library);

    UIRect draw(const char* str);
    UIRect draw(const char* str, const uint16_t canvas_width_px);

    /**
     * Set the font size in pixels
     */
    void set_size(uint16_t size_px);

    /**
     * Compute how wide a string will be in pixels at the current size
     */
    uint16_t compute_px_width(const char* str);

    /**
     * Set the top-left corner of the next draw operation
     */
    inline void move_to(uint16_t x, uint16_t y) {
        m_x = x;
        m_y = y;
    }

    /**
     * Set the text colour as an RGB triplet (0xAABBCC)
     * The upper byte of the 32-bit value is ignored
     */
    inline void set_colour(uint32_t rgb) {
        m_colour = rgb;
    }

    /**
     * Set the fill/background colour as an RGB triplet (0xAABBCC)
     * The upper byte of the 32-bit value is ignored
     */
    inline void set_background(uint32_t rgb) {
        m_background = rgb;
    }

    /**
     * Set how much to thicken glyphs when rendering
     * Font units are 1/64th of a pixel, so a value of 64 = 1px
     */
    inline void set_embolden(uint16_t font_units) {
        m_embolden = font_units;
    }

    /**
     * Set the text colour as an RGB triplet (0xAABBCC)
     * The upper byte of the 32-bit value is ignored
     */
    inline void set_render_mode(RenderMode mode) {
        m_mode = mode;
    }

    inline int16_t x() {
        return m_x;
    }

    inline int16_t y() {
        return m_y;
    }

private:

    FT_Library m_ft_library;

    int16_t m_x;
    int16_t m_y;

    uint32_t m_colour;
    uint32_t m_background;
    uint16_t m_size_px;
    uint16_t m_embolden;

    RenderMode m_mode;

    // Shared data between UIFontPen instances to avoid reloading the same font repeatedly
    static uint8_t* ms_fontdata;
    static FT_Face ms_face;
};

/**
 * Manager of font caching and loading
 */
class FontStore {
public:
    FontStore();
    ~FontStore();

    FT_Error registerFont(const char* path);

    UIFontPen get_pen();
    UIFontPen get_monospace_pen();

    bool drawGlyph(uint32_t codepoint, int adjust_y = 0);

    inline const std::vector<CodepointRange>& codepoint_ranges()
    {
        return m_indexer.ranges();
    }

    inline void optimise()
    {
        shrinkContainer(m_font_table);
        return m_indexer.compressRanges();
    }

    /**
     * Erase the screen region from the most recent call to drawGlyph
     */
    void clearLastGlyph();

    /**
     * Unload any loaded FreeType face to free up heap memory
     */
    void unloadFace();

private:

    /**
     * Load an indexed font by its registered id
     */
    bool loadFace(uint id);

private:

    // Codepoint lookup
    FontIndexer m_indexer;

    // Last drawn region for quick blanking
    UIRect m_last_draw;

    // FreeType state
    FT_Library m_ft_library;

    // Currently loaded font face
    FT_Face m_face;
    uint m_active_id;

    // Table of registered fonts
    std::vector<std::string> m_font_table;
};
