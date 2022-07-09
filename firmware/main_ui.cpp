#include "main_ui.hh"

#include "filesystem.hh"
#include "font.hh"
#include "icons.hh"
#include "st7789.h"
#include "unicode_db.hh"

#include <stdint.h>

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

static void blank_and_invalidate(UIRect &rect)
{
    rect.clamp(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);

    if (rect.is_valid()) {
        st7789_fill_window(0x0, rect.x, rect.y, rect.width, rect.height);
        rect.invalidate();
    }
}

MainUI::MainUI()
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

void MainUI::run_demo()
{
    while (true) {
        for (const auto range : m_fontstore.codepoint_ranges()) {
            for (uint32_t codepoint = range.start; codepoint < range.end; codepoint++) {
                show_codepoint(codepoint);
                sleep_ms(100);
            }
        }
    }
}

void MainUI::show_codepoint(uint32_t codepoint)
{
    static const char* invalid_block = "INVALID BLOCK";
    static const char* unnamed_codepoint = "UNAMED CODEPOINT";
    static const char* missing_glyph = "NO GLYPH";
    static const char* invalid_codepoint = "INVALID CODEPOINT";
    static const char* control_char = "CTRL CODE";

    // Look up codepoint metadata
    const char* block_name = uc_get_block_name(codepoint);
    const char* codepoint_name = uc_get_codepoint_name(codepoint);

    bool should_render_titles = true;

    if (block_name == NULL) {
        block_name = invalid_block;
    }

    if (codepoint_name == NULL) {
        codepoint_name = unnamed_codepoint;
    }

    printf("%d: %s -> %s\n", codepoint, block_name, codepoint_name);

    // Selectively clear anything we last drew last call
    // (except glyphs as those are automatically cleared by FontStore::drawGlyph)
    blank_and_invalidate(m_last_draw);

    if (is_control_char(codepoint)) {
        // Technically valid codepoint, but has no visual representation

        UIFontPen pen = m_fontstore.get_pen();
        pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

        pen.set_size(34);
        pen.set_embolden(128);
        pen.set_colour(kColour_Gray);

        m_fontstore.clearLastGlyph();

        const uint16_t text_width = pen.compute_px_width(control_char);
        pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), ((DISPLAY_HEIGHT)/2) - 20);
        m_last_draw = pen.draw(control_char, text_width);

    } else {
        const bool didDrawGlyph = m_fontstore.drawGlyph(codepoint, 10);

        if (!didDrawGlyph) {
            // No glyph for the codepoint in the selected font

            // Erase the previous glyph, since it's not done automatically on failed drawGlyph calls
            m_fontstore.clearLastGlyph();

            UIFontPen pen = m_fontstore.get_pen();
            pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

            if (block_name == invalid_block) {
                // Invalid codepoint as it's outside any block

                const uint y = (DISPLAY_HEIGHT/2);

                should_render_titles = false;

                // The bad codepoint in hex
                {
                    char _hex_string[12];
                    char* hex_string = (char*) &_hex_string;
                    sprintf(hex_string, "0x%X", codepoint);

                    pen.set_colour(kColour_White);
                    pen.set_size(44);
                    pen.set_embolden(128);

                    const uint16_t text_width = pen.compute_px_width(hex_string);
                    pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), y);
                    pen.draw(hex_string, text_width);
                }

                // Title
                {
                    pen.set_colour(kColour_Error);
                    pen.set_size(18);
                    pen.set_embolden(64);

                    const uint16_t text_width = pen.compute_px_width(invalid_codepoint);
                    pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), y - 22);
                    pen.draw(invalid_codepoint, text_width);
                }

            } else {
                // Valid codepoint, but not in any font we have

                pen.set_size(34);
                pen.set_embolden(128);
                pen.set_colour(kColour_Gray);

                const uint16_t text_width = pen.compute_px_width(missing_glyph);
                pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), ((DISPLAY_HEIGHT)/2) - 20);
                m_last_draw = pen.draw(missing_glyph, text_width);
            }
        } else {
            // Glyph drew successfully - ignore our last text bounds
            m_last_draw.invalidate();
        }
    }

    static UIRect title_area;
    static UIRect value_area;

    if (should_render_titles) {
        blank_and_invalidate(title_area);

        {
            UIFontPen pen = m_fontstore.get_pen();
            pen.set_size(16);

            // Codepoint name
            {
                pen.set_embolden(24); // Thicken the small text a little as it renders grey otherwise

                if (codepoint_name == unnamed_codepoint) {
                    pen.set_colour(kColour_Error);
                } else {
                    pen.set_colour(kColour_White);
                }

                const uint16_t codepoint_text_width = pen.compute_px_width(codepoint_name);
                pen.move_to(std::max(10, (DISPLAY_WIDTH - codepoint_text_width)/2), 23);
                title_area = pen.draw(codepoint_name, codepoint_text_width);
            }

            // Block name
            {
                pen.set_embolden(80);
                if (block_name == invalid_block) {
                    pen.set_colour(kColour_Error);
                } else {
                    pen.set_colour(kColour_BlockName);
                }

                const uint16_t block_text_width = pen.compute_px_width(block_name);
                pen.move_to(std::max(20, (DISPLAY_WIDTH - block_text_width)/2), 0);
                title_area += pen.draw(block_name, block_text_width);
            }
        }

        blank_and_invalidate(value_area);

        {
            UIFontPen pen = m_fontstore.get_monospace_pen();

            {
                const int pull_towards_centre = 10;

                uint16_t text_width;

                char _buf[12];
                char* str = (char*) &_buf;

                // pen.set_render_mode(UIFontPen::kMode_DirectToScreen);
                pen.set_size(16);
                pen.set_embolden(80);

                // Codepoint hex value
                sprintf(str, "U+%02X", codepoint);
                text_width = pen.compute_px_width(str);
                pen.move_to(std::max(0, (((DISPLAY_WIDTH/2) - text_width)/2) + pull_towards_centre), DISPLAY_HEIGHT - 20);
                pen.set_colour(0x70d100);
                value_area = pen.draw(str);

                // Codepoint decimal value
                sprintf(str, "%d", codepoint);
                text_width = pen.compute_px_width(str);
                pen.move_to(std::max(0, (DISPLAY_WIDTH/2) - pull_towards_centre + (((DISPLAY_WIDTH/2) - text_width)/2)), DISPLAY_HEIGHT - 20);
                pen.set_colour(0x8200d1);
                value_area += pen.draw(str);
            }
        }

    } else {
        blank_and_invalidate(title_area);
        blank_and_invalidate(title_area);
    }
}