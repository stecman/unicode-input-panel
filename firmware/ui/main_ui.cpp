#include "main_ui.hh"

#include "filesystem.hh"
#include "font.hh"
#include "icons.hh"
#include "st7789.h"
#include "unicode_db.hh"

#include <stdint.h>
#include <stdlib.h>

#if !PICO_ON_DEVICE
#include <unistd.h>
#define sleep_ms(x)usleep(x * 1000);
#endif

enum UIColour : uint32_t {
    kColour_White = 0xffffff,
    kColour_Gray = 0xa8a8a8,
    kColour_Orange = 0xff8c00,
    kColour_Disabled = 0x1b202d,
    kColour_Error = 0xf02708,
    kColour_BlockName = 0x00bcff,
};

static const char* s_invalid_block = "INVALID BLOCK";
static const char* s_unnamed_codepoint = "UNAMED CODEPOINT";
static const char* s_missing_glyph = "NO GLYPH";
static const char* s_invalid_codepoint = "INVALID CODEPOINT";
static const char* s_control_char = "CTRL CODE";


MainUI::MainUI()
    : m_codepoint(0),
      m_mode(MainUI::kMode_Hex),
      m_shift_lock(false),
      m_codepoint_dirty(true),
      m_block_label(nullptr, 0, 25),
      m_codepoint_label(nullptr, 23, 10)
{
    UIRect erase_rect;

    // Clear screen
    st7789_fill(0);

    ProgressPngImage progress_img = icons_unicode_logo();

    // Draw initial 0% image
    {
        PngImage* image = progress_img.load();

        const uint16_t x = (DISPLAY_WIDTH - image->width) / 2;
        const uint16_t y = (DISPLAY_HEIGHT - image->height) / 2;
        erase_rect = UIRect(x, y, image->width, image->height);

        progress_img.draw_initial(x, y);
    }

    st7789_display_on(true);

    if (fs::mount()) {
        // TODO: Show message on screen
        printf("Failed to mount SD Card\n");
        abort();
    }

    printf("\n\nLoading fonts...\n");

    fs::walkdir("fonts", [&](const char* fontpath, uint8_t progress) {
        m_fontstore.registerFont(fontpath);
        progress_img.update_progress(progress);
    });

    // Join adjacent ranges that use the same font
    //
    // This massively reduces the memory footprint of the index (10-fold), at the cost of not
    // being able to identify missing codepoints with the index. This is fine as we can ask
    // the associated font before rendering if it actually has a glyph.
    m_fontstore.optimise();

    // Show the full logo briefly before switching to the application
    sleep_ms(250);

    // Clear logo from screen to make room for the application
    st7789_fill_window(0, erase_rect.x, erase_rect.y, erase_rect.width, erase_rect.height);
    st7789_deselect();
}

void MainUI::set_low_byte(uint8_t mask)
{
    const uint32_t previous = m_codepoint;

    // Update the low byte of the codepoint
    m_codepoint &= 0xFFFFFF00;
    m_codepoint |= mask;

    if (m_codepoint != previous) {
        m_codepoint_dirty = true;
    }
}

void MainUI::shift()
{
    const uint8_t low_byte = m_codepoint & 0xFF;
    set_codepoint((m_codepoint << 8) | low_byte);
}

void MainUI::set_shift_lock(bool enable)
{
    m_shift_lock = enable;
    m_codepoint_dirty = true;
}

void MainUI::goto_next_mode()
{
    m_mode = static_cast<DisplayMode>(
        static_cast<int>(m_mode) + 1
    );

    if (m_mode >= kMode_END) {
        m_mode = static_cast<DisplayMode>(0);
    }

    m_codepoint_dirty = true;
}

bool MainUI::get_shift_lock()
{
    return m_shift_lock;
}

void MainUI::reset()
{
    set_shift_lock(false);
    set_codepoint(m_codepoint & 0xFF);
}

void MainUI::flush_buffer()
{
    if (!m_shift_lock) {
        reset();
    }
}

void MainUI::set_codepoint(uint32_t codepoint)
{
    if (m_codepoint != codepoint) {
        m_codepoint_dirty = true;
    }

    m_codepoint = codepoint;
}

uint32_t MainUI::get_codepoint()
{
    return m_codepoint;
}

void MainUI::tick()
{
    render();
}

void MainUI::render() {
    if (m_codepoint_dirty) {
        m_codepoint_dirty = false;
        render_codepoint();
    }

    render_scrolling_labels();
}

void MainUI::render_codepoint()
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

    // Render input feedback
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

            if (m_mode == MainUI::kMode_Hex) {
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
                if (m_mode == MainUI::kMode_Hex) {
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

void MainUI::render_scrolling_labels()
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
