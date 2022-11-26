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
    kColour_Error = 0xf02708,
    kColour_BlockName = 0x00bcff,
};

static const char* s_invalid_block = "INVALID BLOCK";
static const char* s_unnamed_codepoint = "UNAMED CODEPOINT";
static const char* s_missing_glyph = "NO GLYPH";
static const char* s_invalid_codepoint = "INVALID CODEPOINT";
static const char* s_control_char = "CTRL CODE";

static void blank_and_invalidate(UIRect &rect)
{
    if (rect.is_valid()) {
        // Limit blanking to actual screen space
        rect.clamp(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);

        // Fill area with black pixels
        st7789_fill_window(0x0, rect.x, rect.y, rect.width, rect.height);

        // Avoid redundant erasures
        rect.invalidate();
    }
}

/**
 * Blank any areas where "next" didn't overlap "last"
 * This currently assumes both rects are the same height as it was written for text labels
 */
static void diff_blank(UIRect &last, UIRect &next)
{
    if (last.is_valid()) {
        last.clamp(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
        next.clamp(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);

        // Clear left-hand side if the last drawing was larger
        if (next.x > last.x) {
            UIRect blanking = last;
            blanking.width = next.x - last.x;
            blanking.draw_outline_debug();
            blank_and_invalidate(blanking);
        }

        // Clear right-hand side if the last drawing was larger
        const int next_end_x = next.x + next.width;
        const int last_end_x = last.x + last.width;
        if (next_end_x  < last_end_x) {
            UIRect blanking = last;
            blanking.x = next_end_x;
            blanking.width = last_end_x - next_end_x;
            blank_and_invalidate(blanking);
        }
    }
}

MainUI::MainUI()
    : m_codepoint(0),
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
}

bool MainUI::get_shift_lock()
{
    return m_shift_lock;
}

void MainUI::reset()
{
    m_shift_lock = false;
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

    bool should_render_titles = true;

    if (block_name == NULL) {
        block_name = s_invalid_block;
    }

    if (codepoint_name == NULL) {
        codepoint_name = s_unnamed_codepoint;
    }

    printf("%d: %s -> %s\n", m_codepoint, block_name, codepoint_name);

    // Selectively clear anything we last drew last call
    // (except glyphs as those are automatically cleared by FontStore::drawGlyph)
    blank_and_invalidate(m_last_draw);

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

                const uint y = (DISPLAY_HEIGHT/2) - 20;

                should_render_titles = false;

                // The bad codepoint in hex
                {
                    char _hex_string[12];
                    char* hex_string = (char*) &_hex_string;
                    sprintf(hex_string, "0x%X", m_codepoint);

                    pen.set_colour(kColour_White);
                    pen.set_size(44);
                    pen.set_embolden(128);

                    const uint16_t text_width = pen.compute_px_width(hex_string);
                    pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), y);
                    m_last_draw = pen.draw(hex_string, text_width);
                }

                // Title
                {
                    pen.set_colour(kColour_Error);
                    pen.set_size(18);
                    pen.set_embolden(64);

                    const uint16_t text_width = pen.compute_px_width(s_invalid_codepoint);
                    pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), y - 22);
                    m_last_draw += pen.draw(s_invalid_codepoint, text_width);
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

    // Render codepoint value
    {
        static UIRect codepoint_area;
        static UIRect decimal_area;

        if (should_render_titles) {
            const int pull_towards_centre = 10;

            //UIFontPen pen = m_fontstore.get_monospace_pen();
            UIFontPen pen = m_fontstore.get_pen();

            char _buf[12];
            char* str = (char*) &_buf;
            uint16_t text_width;

            //pen.set_render_mode(UIFontPen::kMode_DirectToScreen);
            pen.set_size(16);
            pen.set_embolden(80);

            // Codepoint hex value
            sprintf(str, "U+%02X", m_codepoint);
            text_width = pen.compute_px_width(str);
            pen.move_to(std::max(0, (((DISPLAY_WIDTH/2) - text_width)/2) + pull_towards_centre), DISPLAY_HEIGHT - 22);
            pen.set_colour(0x70d100);

            {
                UIRect area(pen.draw(str));
                diff_blank(codepoint_area, area);
                codepoint_area = area;
            }

            // Codepoint decimal value
            sprintf(str, "%d", m_codepoint);
            text_width = pen.compute_px_width(str);
            pen.move_to(std::max(0, (DISPLAY_WIDTH/2) - pull_towards_centre + (((DISPLAY_WIDTH/2) - text_width)/2)), DISPLAY_HEIGHT - 22);
            pen.set_colour(0x8200d1);

            {
                UIRect area(pen.draw(str));
                diff_blank(decimal_area, area);
                decimal_area = area;
            }
        } else {
            blank_and_invalidate(codepoint_area);
            blank_and_invalidate(decimal_area);
        }
    }

    // Render codepoint metadata
    if (should_render_titles) {
        m_block_label.replace(block_name);
        m_codepoint_label.replace(codepoint_name);

    } else {
        m_block_label.clear();
        m_codepoint_label.clear();
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
    blank_and_invalidate(m_last_draw);
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

        diff_blank(m_last_draw, rect);

        m_last_draw = rect;
    }
}
