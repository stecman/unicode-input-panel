#pragma once

#include "font_indexer.hh"
#include "util.hh"

// FreeType
#include "ft2build.h"
#include FT_CACHE_H
#include FT_FREETYPE_H

// C++
#include <string>
#include <vector>

#include <stdint.h>

/**
 * Screen region for passing around blanking/erase information
 */
struct UIRect {
    UIRect()
        : x(0), y(0),
          width(0), height(0) {}

    UIRect(int16_t x, int16_t y, int16_t w, int16_t h)
        : x(x), y(y),
          width(w), height(h) {}

    inline bool is_valid() const
    {
        return width != 0 && height != 0;
    }

    inline void invalidate()
    {
        width = 0;
        height = 0;
    }

    void clamp(int16_t min_x, int16_t min_y, int16_t max_x, int16_t max_y);

    void merge(const UIRect &other);
    UIRect& operator+=(const UIRect& other);

    void draw_outline_debug(uint32_t colour = 0xFF0000) const;

    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
};

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

    UIFontPen(const uint8_t* fontdata, size_t length, FT_Library library);
    ~UIFontPen();

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
    FT_Face m_face;
    FT_Error m_error;

    int16_t m_x;
    int16_t m_y;

    uint32_t m_colour;
    uint32_t m_background;
    uint16_t m_size_px;
    uint16_t m_embolden;

    RenderMode m_mode;
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

    /// Permanently loaded in-memory face for UI
    FT_Face m_ui_face;

    // Currently loaded font face
    FT_Face m_face;
    uint m_active_id;

    // Table of registered fonts
    std::vector<std::string> m_font_table;
};
