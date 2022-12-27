#include "utf8_view.hh"

#include "st7789.h"
#include "unicode_db.hh"
#include "util.hh"

/**
 * Guess sequence length from a potentially incomplete first byte
 * Doesn't do any validation, so the encoding might be junk
 */
static uint32_t guess_encoding_length(uint8_t byte0)
{
    byte0 >>= 3;
    if (byte0 == 0b11110) {
        return 4;
    }

    byte0 >>= 1;
    if (byte0 == 0b1110) {
        return 3;
    }
    
    byte0 >>= 1;
    if (byte0 == 0b110) {
        return 2;
    }

    return 1;
}

/**
 * Format a byte as a binary literal (eg. "0b10011001")
 * Sets the <out> parameter to a null terminated string
 */
void format_binary_literal(uint8_t byte, char out[9])
{
    uint i = 8;
    out += i;

    while (i--) {
        (*--out) = 0x30 + (byte & 1);
        byte >>= 1;
    }

    out[8] = '\0';
}

UTF8View::UTF8View(FontStore& fontstore)
    : m_title_display(fontstore),
      m_glyph_display(fontstore, DISPLAY_WIDTH - 20, DISPLAY_HEIGHT - 90, 0),
      m_fontstore(fontstore) {}

void UTF8View::set_low_byte(uint8_t value)
{
    // Update the current byte being edited
    m_buffer[m_index] = value;
    m_dirty = true;
}

void UTF8View::shift()
{
    const uint8_t current = m_buffer[m_index];

    // Move to the next byte
    m_index++;
    if (m_index >= guess_encoding_length(m_buffer[0])) {
        m_index = 0;
    }

    // Set the new byte to the current switch positions
    m_buffer[m_index] = current;

    m_dirty = true;
}

void UTF8View::toggle_shift_lock()
{
    m_shift_lock = !m_shift_lock;
    m_dirty = true;
}

void UTF8View::flush_buffer()
{
    if (!m_shift_lock) {
        reset();
    }
}

void UTF8View::reset()
{
    m_shift_lock = false;

    m_buffer[0] = m_buffer[m_index];
    m_buffer[1] = 0;
    m_buffer[2] = 0;
    m_buffer[3] = 0;

    m_index = 0;
    m_dirty = true;
}

const std::vector<uint32_t> UTF8View::get_codepoints()
{
    uint32_t codepoint = utf8_to_codepoint(m_buffer);

    if (codepoint == kInvalidEncoding) {
        return {};
    }

    return { codepoint };
}

void UTF8View::render()
{
    if (m_dirty) {
        m_dirty = false;

        const uint32_t codepoint = utf8_to_codepoint(m_buffer);

        if (codepoint == kInvalidEncoding) {
            m_small_help.blank_and_invalidate();
            m_glyph_display.clear();

            render_invalid_banner();
            render_large_input_help();
            
        } else {
            const char* block_name = uc_get_block_name(codepoint);
            const char* codepoint_name = uc_get_codepoint_name(codepoint);
            const bool is_valid = block_name != nullptr;

            m_large_help.blank_and_invalidate();

            m_invalid_encoding.blank_and_invalidate();
            m_glyph_display.draw(codepoint, is_valid);
            m_title_display.update_labels(block_name, codepoint_name);

            render_small_input_help();
        }

        render_mode_bar();
    }

    m_title_display.render();
}

void UTF8View::render_invalid_banner()
{
    static const char* s_invalid_encoding = "INVALID ENCODING";

    if (!m_invalid_encoding.is_valid())
    {
        m_title_display.clear();

        st7789_fill_window_colour(0x636363, 0, 0, DISPLAY_WIDTH, 30);
        m_invalid_encoding = UIRect(0, 0, DISPLAY_WIDTH, 30);

        UIFontPen pen = m_fontstore.get_pen();
        pen.set_render_mode(UIFontPen::kMode_DirectToScreen);
        pen.set_colour(0);
        pen.set_background(0x636363);
        pen.set_size(18);
        pen.set_embolden(64);

        const uint16_t text_width = pen.compute_px_width(s_invalid_encoding);
        pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), 3);
        pen.draw(s_invalid_encoding, text_width);
    }
}

void UTF8View::render_large_input_help()
{
    char _buf[12];
    uint16_t text_width;

    const int line_height = 45;
    const uint length = guess_encoding_length(m_buffer[0]);

    if (length != m_last_length) {
        m_large_help.blank_and_invalidate();
    }

    UIFontPen pen = m_fontstore.get_monospace_pen();
    pen.set_render_mode(UIFontPen::kMode_CanvasBuffer);
    pen.set_size(36);
    pen.set_embolden(80);

    const int32_t pullup = (length * line_height) / 2;
    const int32_t start_y = DISPLAY_HEIGHT/2 - pullup + 5;

    for (uint i = 0; i < length; i++){
        char* str = (char*) &_buf;
        format_binary_literal(m_buffer[i], str);
        text_width = pen.compute_px_width(str);

        pen.move_to(DISPLAY_WIDTH/2 - text_width/2, start_y + (i*line_height));
        render_byte(pen, i, str, text_width, m_large_help);
    }

    m_last_length = length;
}

void UTF8View::render_small_input_help()
{
    char _buf[12];
    uint16_t text_width;

    const uint length = guess_encoding_length(m_buffer[0]);

    if (length != m_last_length) {
        m_small_help.blank_and_invalidate();
    }

    UIFontPen pen = m_fontstore.get_monospace_pen();
    pen.set_render_mode(UIFontPen::kMode_CanvasBuffer);
    pen.set_size(11);
    pen.set_embolden(20);

    const int start_x = (DISPLAY_WIDTH - (57 * length)) / 2;
    const int spacing = 5;
    pen.move_to(start_x - spacing, DISPLAY_HEIGHT - 40);

    for (uint i = 0; i < length; i++){
        char* str = (char*) &_buf;
        format_binary_literal(m_buffer[i], str);
        text_width = pen.compute_px_width(str);

        pen.move_to(pen.x() + spacing, pen.y());
        render_byte(pen, i, str, text_width, m_small_help);
    }

    m_last_length = length;
}

/**
 * Render a base-two string using the passed pen
 * The caller should set up the position and font size before calling
 */
void UTF8View::render_byte(UIFontPen &pen, uint index, char* str, uint16_t text_width, UIRect &painted)
{
    const uint8_t byte = m_buffer[index];

    const uint32_t error_colour = index == m_index ? kColour_Error : 0xbd5141;
    const uint32_t base_colour = index == m_index ? kColour_White : kColour_Gray;

    if (index == 0) {
        // First byte
        if (byte & 0x80) {
            // Most significant bit is set, which indicates a multi-byte sequence
            if (is_utf8_continuation(byte)) {
                // First byte is not allowed to be a continuation
                pen.set_colour(error_colour);
                painted += pen.draw_length(str, 2);
                str += 2;

            } else if ((byte & 0xF0) == 0xF0) {
                // 4 byte sequence
                if ((byte & 0x08) != 0) {
                    // Missing trailing zero
                    pen.set_colour(base_colour);
                    painted += pen.draw_length(str, 4);
                    str += 4;

                    pen.set_colour(error_colour);
                    painted += pen.draw_length(str, 1);
                    str += 1;
                }

            } else if ((byte & 0xE0) == 0xE0) {
                // 3 byte sequence
                if ((byte & 0x10) != 0) {
                    // Missing trailing zero
                    pen.set_colour(base_colour);
                    painted += pen.draw_length(str, 3);
                    str += 3;

                    pen.set_colour(error_colour);
                    painted += pen.draw_length(str, 1);
                    str += 1;
                }

            } else if ((byte & 0xC0) == 0xC0) {
                // 2 byte sequence
                if ((byte & 0x20) != 0) {
                    // Missing trailing zero
                    pen.set_colour(base_colour);
                    painted += pen.draw_length(str, 2);
                    str += 2;

                    pen.set_colour(error_colour);
                    painted += pen.draw_length(str, 1);
                    str += 1;
                }
            }

            // Draw the rest of the string in white
            pen.set_colour(base_colour);
            painted += pen.draw(str, text_width);

        } else {
            // No validation issues
            pen.set_colour(base_colour);
            painted += pen.draw(str, text_width);
        }

    } else {
        // Continuation byte
        if (!is_utf8_continuation(byte)) {
            // Highlight which of the continuation bits is wrong
            pen.set_colour((byte & 0x80) != 0 ? base_colour : error_colour);
            painted += pen.draw_length(str, 1);
            str++;

            pen.set_colour((byte & 0x40) == 0 ? base_colour : error_colour);
            painted += pen.draw_length(str, 1);
            str++;

            // Draw the rest of the string
            pen.set_colour(base_colour);
            painted += pen.draw(str);
        } else {
            // No validation issues
            pen.set_colour(base_colour);
            painted += pen.draw(str, text_width);
        }
    }
}

void UTF8View::render_mode_bar()
{
    char _buf[12];
    char* str = (char*) &_buf;
    uint16_t text_width;

    // Hex value in buffer encoding
    {
        const uint32_t length = guess_encoding_length(m_buffer[0]);

        UIFontPen pen = m_fontstore.get_monospace_pen();
        pen.set_render_mode(UIFontPen::kMode_CanvasBuffer);
        pen.set_size(20);
        pen.set_embolden(80);

        for (uint32_t i = 0; i < length; i++) {
            sprintf(str + (i*2), "%02X", m_buffer[i]);
        }

        text_width = pen.compute_px_width(str);
        
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
            static const char* _utf8_text = "UTF-8";
            pen.set_colour(0xbb07ff);
            pen.move_to(20, DISPLAY_HEIGHT - 20);
            m_mode_bar_draw += pen.draw(_utf8_text);
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

std::vector<uint8_t> UTF8View::get_buffer()
{
    std::vector<uint8_t> bytes;
    bytes.reserve(4);

    const uint32_t length = guess_encoding_length(m_buffer[0]);
    for (uint32_t i = 0; i < length; i++) {
        bytes.push_back(m_buffer[i]);
    }

    return bytes;
}

void UTF8View::clear()
{
    m_title_display.clear();
    m_glyph_display.clear();

    m_invalid_encoding.blank_and_invalidate();
    m_codepoint_value_draw.blank_and_invalidate();
    m_small_help.blank_and_invalidate();
    m_large_help.blank_and_invalidate();
    m_mode_bar_draw.blank_and_invalidate();
}