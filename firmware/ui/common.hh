#pragma once

#include <stdint.h>

// Forward declarations
class UIFontPen;
class FontStore;

enum UIColour : uint32_t {
    kColour_White = 0xffffff,
    kColour_Gray = 0xa8a8a8,
    kColour_Orange = 0xff8c00,
    kColour_Disabled = 0x1b202d,
    kColour_Error = 0xf02708,
    kColour_BlockName = 0x00bcff,
};

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

    /**
     * Clear area on screen and flag so further calls will be a no-op
     */
    void blank_and_invalidate(uint8_t fill = 0x0);

    /**
     * Blank any areas where "next" doesn't overlap the existing area
     * This currently assumes both rects are the same height as it was written for text labels
     */
    void diff_blank(UIRect &next, uint8_t fill = 0x0);

    void draw_outline_debug(uint32_t colour = 0xFF0000) const;

    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
};


/**
 * Line of text that automatically scrolls if too wide to fit on screen
 */
class ScrollingLabel {
public:

    ScrollingLabel();

    ScrollingLabel(const char* text, int y = 0, int padding = 0);

    /**
     * Set the label contents to null and blank the last drawn region
     */
    void clear();

    /**
     * Change the text and reset the scroll of this label
     */
    void replace(const char* text);

    /**
     * Draw the next frame of the label to screen
     */
    void render(UIFontPen &pen);

    inline const char* value()
    {
        return m_str;
    }

private:

    enum AnimationState {
        kState_New,
        kState_Fixed,
        kState_WaitingLeft,
        kState_Animating,
        kState_WaitingRight,
        kState_AnimatingReset,
    };

    const char* m_str;
    int m_y;
    int m_padding;

    int16_t m_x;
    int16_t m_start_x;
    int16_t m_end_x;
    int16_t m_width;

    uint32_t m_tick;
    uint32_t m_next_tick;
    AnimationState m_state;

    UIRect m_last_draw;
};

/**
 *Screen-wide header displaying codepoint and block name
 */
class CodepointTitle
{
public:
    CodepointTitle(FontStore& fontstore);

    void update_labels(const char* block_name, const char* codepoint_name);

    /**
     * Render any changes, and move scrolling labels
     * This should be called once per frame
     */
    void render();

    // Blank previously rendered
    void clear();

private:
    FontStore& m_fontstore;

    UIRect m_title_draw;
    ScrollingLabel m_block_label;
    ScrollingLabel m_codepoint_label;
};

/**
 * Draw big a glyph or feedback for non-representable codepoints
 *
 * This is expensive, so should be managed by the caller to only
 * be called when the codepoint needs re-drawing
 */
void draw_codepoint(uint32_t codepoint);