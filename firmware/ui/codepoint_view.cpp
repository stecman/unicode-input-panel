#include "codepoint_view.hh"

#include "st7789.h"
#include "unicode_db.hh"

static const char* s_invalid_block = "INVALID BLOCK";
static const char* s_unnamed_codepoint = "UNAMED CODEPOINT";
static const char* s_missing_glyph = "NO GLYPH";
static const char* s_invalid_codepoint = "INVALID CODEPOINT";
static const char* s_control_char = "CTRL CODE";

CodepointView::CodepointView(FontStore& fontstore)
    : m_block_label(nullptr, 0, 25),
      m_codepoint_label(nullptr, 23, 10),
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
            render_glyph();
            m_last_codepoint = m_codepoint;
        }

        render_input_feedback();
    }

    render_scrolling_labels();
}

void CodepointView::render_glyph()
{
    // Look up codepoint metadata
    const char* block_name = uc_get_block_name(m_codepoint);
    const char* codepoint_name = uc_get_codepoint_name(m_codepoint);

    bool valid_codepoint = true;

    if (block_name == NULL) {
        block_name = s_invalid_block;
    }

    if (codepoint_name == NULL) {
        codepoint_name = s_unnamed_codepoint;
    }

    printf("%d: %s -> %s\n", m_codepoint, block_name, codepoint_name);

    // Selectively clear anything we last drew last call
    // (except glyphs as those are automatically cleared by FontStore::drawGlyph)
    m_last_draw.blank_and_invalidate();

    if (is_control_char(m_codepoint)) {
        // Technically valid codepoint, but has no visual representation

        UIFontPen pen = m_fontstore.get_pen();
        pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

        pen.set_size(34);
        pen.set_embolden(128);
        pen.set_colour(kColour_Gray);

        m_fontstore.clearLastGlyph();

        const uint16_t text_width = pen.compute_px_width(s_control_char);
        pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), ((DISPLAY_HEIGHT)/2) - 20);
        m_last_draw = pen.draw(s_control_char, text_width);

    } else {
        const bool didDrawGlyph = m_fontstore.drawGlyph(m_codepoint, 10);

        if (!didDrawGlyph) {
            // No glyph for the codepoint in the selected font

            // Erase the previous glyph, since it's not done automatically on failed drawGlyph calls
            m_fontstore.clearLastGlyph();

            UIFontPen pen = m_fontstore.get_pen();
            pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

            if (block_name == s_invalid_block) {
                // Invalid codepoint as it's outside any block
                valid_codepoint = false;

                // The bad codepoint in hex
                {
                    char _hex_string[12];
                    char* hex_string = (char*) &_hex_string;
                    sprintf(hex_string, "0x%X", m_codepoint);

                    // Adjust font size to fit on screen
                    int yOffset;
                    if (0xFF000000 & m_codepoint) {
                        pen.set_size(32);
                        yOffset = 22;
                    } else {
                        pen.set_size(44);
                        yOffset = 28;
                    }

                    pen.set_colour(kColour_Gray);
                    pen.set_embolden(128);

                    const uint16_t text_width = pen.compute_px_width(hex_string);
                    pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), (DISPLAY_HEIGHT/2) - yOffset);
                    m_last_draw = pen.draw(hex_string, text_width);
                }

            } else {
                // Valid codepoint, but not in any font we have

                pen.set_size(34);
                pen.set_embolden(128);
                pen.set_colour(kColour_Gray);

                const uint16_t text_width = pen.compute_px_width(s_missing_glyph);
                pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), ((DISPLAY_HEIGHT)/2) - 20);
                m_last_draw = pen.draw(s_missing_glyph, text_width);
            }
        } else {
            // Glyph drew successfully - ignore our last text bounds
            m_last_draw.invalidate();
        }
    }

    // Render codepoint metadata
    if (valid_codepoint) {
        m_title_draw.blank_and_invalidate();
        m_block_label.replace(block_name);
        m_codepoint_label.replace(codepoint_name);

    } else {
        // "Invalid codepoint" title
        // This draws over the previous titles
        if (!m_title_draw.is_valid())
        {
            m_block_label.clear();
            m_codepoint_label.clear();

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
    }
}

void CodepointView::render_input_feedback()
{
    static UIRect value_area;

    char _buf[12];
    char* str = (char*) &_buf;
    uint16_t text_width;

    // Codepoint value
    {
        UIFontPen pen = m_fontstore.get_monospace_pen();
        // UIFontPen pen = m_fontstore.get_pen();
        pen.set_render_mode(UIFontPen::kMode_DirectToScreen);
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

        value_area.blank_and_invalidate();
        value_area = pen.draw(str, text_width);
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
            pen.draw(text);
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
            pen.draw(_lock_text);
        }

    }
}

void CodepointView::render_scrolling_labels()
{
    UIFontPen pen = m_fontstore.get_pen();
    pen.set_size(16);

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
