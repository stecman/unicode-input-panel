#include "common.hh"

#include "st7789.h"
#include "ui/font.hh"
#include "unicode_db.hh"

#include <algorithm>

//
// UIRect
//

void UIRect::clamp(int16_t min_x, int16_t min_y, int16_t max_x, int16_t max_y)
{
    const int16_t x2 = std::min(static_cast<int16_t>(x + width), max_x);
    const int16_t y2 = std::min(static_cast<int16_t>(y + height), max_y);

    x = std::max(x, min_x);
    y = std::max(y, min_y);

    width = x2 - x;
    height = y2 - y;
}

void UIRect::merge(const UIRect &other)
{
    if (!is_valid()) {
        *this = other;
        return;
    }

    const int16_t x2 = std::max(x + width, other.x + other.width);
    const int16_t y2 = std::max(y + height, other.y + other.height);

    x = std::min(x, other.x);
    y = std::min(y, other.y);
    width = x2 - x;
    height = y2 - y;
}

UIRect& UIRect::operator+=(const UIRect& other)
{
    merge(other);
    return *this;
}

void UIRect::draw_outline_debug(uint32_t colour) const
{
    if (!is_valid()) {
        return;
    }

    UIRect clamped = *this;
    clamped.clamp(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    for (uint16_t draw_y = clamped.y; draw_y < (clamped.y + height - 1); draw_y++) {
        st7789_set_cursor(clamped.x, draw_y);
        st7789_put(colour);

        st7789_set_cursor(clamped.x + clamped.width - 1, draw_y);
        st7789_put(colour);
    }

    for (uint16_t draw_x = clamped.x; draw_x < (clamped.x + clamped.width - 1); draw_x++) {
        st7789_set_cursor(draw_x, clamped.y);
        st7789_put(colour);

        st7789_set_cursor(draw_x, clamped.y + clamped.height - 1);
        st7789_put(colour);
    }
}

void UIRect::blank_and_invalidate(uint8_t fill)
{
    if (is_valid()) {
        // Limit blanking to actual screen space
        // This modifies the area, but it doesn't matter as it will be invalidated next
        clamp(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

        // Fill area with black pixels
        st7789_fill_window(fill, x, y, width, height);

        // Avoid redundant erasures
        invalidate();
    }
}

void UIRect::diff_blank(UIRect &next, uint8_t fill)
{
    if (this->is_valid()) {
        this->clamp(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
        next.clamp(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);

        // Clear left-hand side if the last drawing was larger
        if (next.x > this->x) {
            UIRect left(x, y, next.x - this->x, height);
            left.blank_and_invalidate(fill);
        }

        // Clear right-hand side if the last drawing was larger
        const int next_end_x = next.x + next.width;
        const int last_end_x = this->x + this->width;
        if (next_end_x  < last_end_x) {
            UIRect right(next_end_x, y, last_end_x - next_end_x, height);
            right.blank_and_invalidate(fill);
        }
    }
}


//
// ScrollingLabel
//

ScrollingLabel::ScrollingLabel()
    : m_str(nullptr) {}

ScrollingLabel::ScrollingLabel(const char* text, int y, int padding)
    : m_str(text),
      m_y(y),
      m_padding(padding),
      m_tick(0),
      m_next_tick(0),
      m_state(ScrollingLabel::kState_New) {}

void ScrollingLabel::replace(const char* text)
{
    m_str = text;
    m_tick = 0;
    m_next_tick = 0;
    m_state = ScrollingLabel::kState_New;
}

void ScrollingLabel::clear()
{
    m_last_draw.blank_and_invalidate();
    m_str = nullptr;
}

void ScrollingLabel::render(UIFontPen &pen)
{
    if (m_str == nullptr) {
        // Nothing to do
        return;
    }

    bool needs_render = false;

    switch (m_state) {
        case ScrollingLabel::kState_New: {
            m_width = pen.compute_px_width(m_str);
            m_start_x = std::max(m_padding, (DISPLAY_WIDTH - m_width)/2);
            m_x = m_start_x;

            if (m_width > (DISPLAY_WIDTH - m_padding*2)) {
                m_end_x = (DISPLAY_WIDTH) - m_width - m_padding;
                m_state = ScrollingLabel::kState_WaitingLeft;
                m_next_tick = 30;
            } else {
                // No movement necessary
                m_state = ScrollingLabel::kState_Fixed;
            }

            needs_render = true;

            break;
        }

        case ScrollingLabel::kState_Fixed:
            // Nothing to do
            return;

        case ScrollingLabel::kState_WaitingLeft: {
            if (m_tick < m_next_tick) {
                m_tick++;
            } else {
                m_state = ScrollingLabel::kState_Animating;
            }

            break;
        }

        case ScrollingLabel::kState_Animating: {
            // Animate scroll
            m_x -= 2;
            needs_render = true;

            if (m_x <= m_end_x) {
                m_state = ScrollingLabel::kState_WaitingRight;
                m_x = m_end_x;
                m_next_tick += 60;
            }

            break;
        }

        case ScrollingLabel::kState_WaitingRight: {
            if (m_tick < m_next_tick) {
                m_tick++;
            } else {
                m_state = ScrollingLabel::kState_AnimatingReset;
            }

            break;
        }

        case ScrollingLabel::kState_AnimatingReset: {
            // Animate quickly back to start position
            int16_t delta = abs(m_x - m_start_x) / 8;
            if (delta < 4) {
                delta = 4;
            }

            m_x += delta;
            needs_render = true;

            if (m_x >= m_start_x) {
                m_state = ScrollingLabel::kState_WaitingLeft;
                m_x = m_start_x;
                m_next_tick += 60;
            }

            break;
        }
    }

    if (needs_render) {
        pen.move_to(m_x, m_y);
        UIRect rect(pen.draw(m_str, m_width));

        m_last_draw.diff_blank(rect);

        m_last_draw = rect;
    }
}

//
// CodepointTitle
//

static const char* s_invalid_block = "INVALID BLOCK";
static const char* s_unnamed_codepoint = "UNAMED CODEPOINT";
static const char* s_invalid_codepoint = "INVALID CODEPOINT";

CodepointTitle::CodepointTitle(FontStore& fontstore)
    : m_fontstore(fontstore),
      m_block_label(nullptr, 0, 25),
      m_codepoint_label(nullptr, 23, 10),
      m_hidden(true) {}

void CodepointTitle::update_labels(const char* block_name, const char* codepoint_name)
{
    m_hidden = false;

    if (block_name == NULL && codepoint_name == NULL) {
        // Invalid codepoint: a banner will be drawn indicating this next render
        m_block_label.clear();
        m_codepoint_label.clear();
        return;
    }

    if (block_name == NULL) {
        block_name = s_invalid_block;
    }

    if (codepoint_name == NULL) {
        codepoint_name = s_unnamed_codepoint;
    }
    
    m_block_label.replace(block_name);
    m_codepoint_label.replace(codepoint_name);
}

void CodepointTitle::clear()
{
    m_title_draw.blank_and_invalidate();

    m_block_label.clear();
    m_codepoint_label.clear();

    m_hidden = true;
}

void CodepointTitle::render()
{
    if (m_hidden) {
        // Not drawing currently
        return;
    }

    if (m_block_label.value() == NULL) {

        // Draw the "invalid codepoint" banner if it hasn't been drawn yet
        // This completely covers the labels, so they don't need to be cleared first
        if (!m_title_draw.is_valid())
        {
            st7789_fill_window_colour(kColour_Error, 0, 0, DISPLAY_WIDTH, 30);
            m_title_draw = UIRect(0, 0, DISPLAY_WIDTH, 30);

            UIFontPen pen = m_fontstore.get_pen();
            pen.set_render_mode(UIFontPen::kMode_DirectToScreen);
            pen.set_colour(kColour_White);
            pen.set_background(kColour_Error);
            pen.set_size(18);
            pen.set_embolden(64);

            const uint16_t text_width = pen.compute_px_width(s_invalid_codepoint);
            pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), 3);
            pen.draw(s_invalid_codepoint, text_width);
        }

    } else {

        // Draw the scrolling labels
        UIFontPen pen = m_fontstore.get_pen();
        pen.set_size(16);

        // Clear any previous banner that was shown
        // (Done after font loading to minimise flicker)
        m_title_draw.blank_and_invalidate();

        {
            pen.set_embolden(24); // Thicken the small text a little as it renders grey otherwise

            if (m_codepoint_label.value() == s_unnamed_codepoint) {
                pen.set_colour(kColour_Error);
            } else {
                pen.set_colour(kColour_White);
            }

            m_codepoint_label.render(pen);
        }

        {
            pen.set_embolden(80);

            if (m_block_label.value() == s_invalid_block) {
                pen.set_colour(kColour_Error);
            } else {
                pen.set_colour(kColour_BlockName);
            }

            m_block_label.render(pen);
        }
    }
}