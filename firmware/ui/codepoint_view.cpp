#include "codepoint_view.hh"

#include "st7789.h"
#include "unicode_db.hh"

CodepointView::CodepointView(FontStore& fontstore)
    : m_title_display(fontstore),
      m_glyph_display(fontstore, DISPLAY_WIDTH - 20, DISPLAY_HEIGHT - 70, 10),
      m_fontstore(fontstore) {}

void CodepointView::set_low_byte(uint8_t mask)
{
    const uint32_t previous = m_codepoint;

    // Update the low byte of the codepoint
    m_codepoint &= 0xFFFFFF00;
    m_codepoint |= mask;

    m_dirty = true;
}

void CodepointView::shift()
{
    const uint8_t low_byte = m_codepoint & 0xFF;
    m_codepoint = (m_codepoint << 8) | low_byte;
    m_dirty = true;
}

void CodepointView::toggle_shift_lock()
{
    m_shift_lock = !m_shift_lock;
    m_dirty = true;
}

void CodepointView::flush_buffer()
{
    if (!m_shift_lock) {
        reset();
    }
}

void CodepointView::reset()
{
    m_shift_lock = false;
    m_codepoint = m_codepoint & 0xFF;
    m_dirty = true;
}

const std::vector<uint32_t> CodepointView::get_codepoints()
{
    return { m_codepoint };
}

bool CodepointView::goto_next_mode()
{
    m_dirty = true;
    m_mode = static_cast<DisplayMode>(
        static_cast<int>(m_mode) + 1
    );

    if (m_mode >= kMode_END) {
        m_mode = static_cast<DisplayMode>(0);

        // Wrapped
        return false;
    }

    // Didn't wrap
    return true;
}

void CodepointView::render() {
    if (m_dirty) {
        m_dirty = false;

        if (m_codepoint != m_last_codepoint) {
            const char* block_name = uc_get_block_name(m_codepoint);
            const char* codepoint_name = uc_get_codepoint_name(m_codepoint);
            const bool is_valid = block_name != nullptr;
            
            m_glyph_display.draw(m_codepoint, is_valid);
            m_title_display.update_labels(block_name, codepoint_name);

            m_last_codepoint = m_codepoint;
        }

        render_input_feedback();
    }

    m_title_display.render();
}

void CodepointView::render_input_feedback()
{
    char _buf[12];
    char* str = (char*) &_buf;
    uint16_t text_width;

    // Codepoint value
    {
        UIFontPen pen = m_fontstore.get_monospace_pen();
        // UIFontPen pen = m_fontstore.get_pen();
        pen.set_render_mode(UIFontPen::kMode_CanvasBuffer);
        pen.set_size(20);
        pen.set_embolden(80);

        if (m_mode == CodepointView::kMode_Hex) {
            // Codepoint hex value
            sprintf(str, "U+%02X", m_codepoint);
            text_width = pen.compute_px_width(str);
        } else {
            // Decimal value
            sprintf(str, "%u", m_codepoint);
            text_width = pen.compute_px_width(str);
        }
        
        pen.move_to(DISPLAY_WIDTH/2 - text_width/2, DISPLAY_HEIGHT - 24);

        UIRect area = pen.draw(str, text_width);
        m_codepoint_value_draw.diff_blank(area);
        m_codepoint_value_draw = area;
    }

    // Flags
    {
        UIFontPen pen = m_fontstore.get_pen();
        pen.set_size(12);
        pen.set_embolden(40);

        // Current mode
        {
            const char* text;
            if (m_mode == CodepointView::kMode_Hex) {
                static const char* _hex_text = "HEX";
                pen.set_colour(0x55b507);
                text = _hex_text;
            } else {
                static const char* _dec_text = "DEC";
                pen.set_colour(0x0b89c7);
                text = _dec_text;
            }

            pen.move_to(22, DISPLAY_HEIGHT - 20);
            m_mode_bar_draw += pen.draw(text);
        }

        // Shift lock status
        {
            static const char* _lock_text = "LOCK";

            if (m_shift_lock) {
                pen.set_colour(kColour_Orange);
            } else {
                pen.set_colour(kColour_Disabled);
            }

            pen.move_to(DISPLAY_WIDTH - 51, DISPLAY_HEIGHT - 20);
            m_mode_bar_draw += pen.draw(_lock_text);
        }

    }
}

std::vector<uint8_t> CodepointView::get_buffer()
{
    return {
        static_cast<uint8_t>(m_codepoint >> 24),
        static_cast<uint8_t>(m_codepoint >> 16),
        static_cast<uint8_t>(m_codepoint >> 8),
        static_cast<uint8_t>(m_codepoint)
    };
}

void CodepointView::clear()
{
    m_title_display.clear();
    m_glyph_display.clear();

    m_last_draw.blank_and_invalidate();
    m_mode_bar_draw.blank_and_invalidate();

    m_last_codepoint = kInvalidEncoding;
}