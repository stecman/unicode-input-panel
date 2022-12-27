#include "numeric_view.hh"

#include "st7789.h"

NumericView::NumericView(FontStore& fontstore)
    : m_fontstore(fontstore) {}

void NumericView::set_low_byte(uint8_t value)
{
    // Update the low byte of the buffer
    m_value &= 0xFFFFFF00;
    m_value |= value;

    m_dirty = true;
}

void NumericView::shift()
{
    const uint8_t low_byte = m_value & 0xFF;
    m_value = (m_value << 8) | low_byte;
    m_dirty = true;
}

void NumericView::set_shift_lock(bool enabled)
{
    m_shift_lock = enabled;
    m_dirty = true;
}

void NumericView::flush_buffer()
{
    if (!m_shift_lock) {
        reset();
    }
}

void NumericView::reset()
{
    m_value = m_value & 0xFF;
    m_dirty = true;
}

const std::vector<uint32_t> NumericView::get_codepoints()
{
    std::vector<uint32_t> codepoints;

    char _buf[12];
    char* str = (char*) &_buf;
    sprintf(str, "0x%02X", m_value);

    while (*str != '\0') {
        codepoints.push_back(*str);
        str++;
    }

    return codepoints;
}

void NumericView::render() {
    if (m_dirty) {
        m_dirty = false;

        render_value();
        render_mode_bar();
    }
}

void NumericView::render_value()
{
    UIFontPen pen = m_fontstore.get_monospace_pen();
    pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

    char _hex_string[12];
    char* hex_string = (char*) &_hex_string;
    sprintf(hex_string, "%02X", m_value);

    // Adjust font size to fit on screen
    int yPos;
    if (m_value > 0xFFFFFF) {
        pen.set_size(50);
        yPos = 100;
    } else if (m_value > 0xFFFF) {
        pen.set_size(66);
        yPos = 90;
    } else if (m_value > 0xFF) {
        pen.set_size(100);
        yPos = 70;
    } else {
        pen.set_size(200);
        yPos = 5;
    }

    pen.set_embolden(128);

    const uint16_t text_width = pen.compute_px_width(hex_string);
    pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), yPos);

    m_last_draw.blank_and_invalidate();
    m_last_draw = pen.draw(hex_string, text_width);
}

void NumericView::render_mode_bar()
{
    char _buf[12];
    char* str = (char*) &_buf;
    uint16_t text_width;

    // Flags
    {
        UIFontPen pen = m_fontstore.get_pen();
        pen.set_size(12);
        pen.set_embolden(40);

        // Current mode
        {
            static const char* _mode_text = "LITERAL";
            pen.set_colour(0xf6c200);

            pen.move_to(20, DISPLAY_HEIGHT - 20);
            m_mode_bar_draw += pen.draw(_mode_text);
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

std::vector<uint8_t> NumericView::get_buffer()
{
    return {
        static_cast<uint8_t>(m_value >> 24),
        static_cast<uint8_t>(m_value >> 16),
        static_cast<uint8_t>(m_value >> 8),
        static_cast<uint8_t>(m_value)
    };
}

void NumericView::clear()
{
    m_last_draw.blank_and_invalidate();
    m_mode_bar_draw.blank_and_invalidate();
}