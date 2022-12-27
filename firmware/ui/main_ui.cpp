#include "main_ui.hh"

#include "filesystem.hh"
#include "st7789.h"
#include "ui/codepoint_view.hh"
#include "ui/icons.hh"
#include "ui/utf8_view.hh"

#include <stdint.h>
#include <stdlib.h>

#if !PICO_ON_DEVICE
#include <unistd.h>
#define sleep_ms(x)usleep(x * 1000);
#endif

// Font lookup for application
static FontStore s_fontstore;

// Available views to cycle through
// These are created at initialisation so we don't have to deal with
// heap allocation between rendering potentially fragmenting the heap.
static UIDelegate* s_views[] = {
    new CodepointView(s_fontstore),
    new UTF8View(s_fontstore),
};
static size_t s_num_views = sizeof(s_views) / sizeof(UIDelegate*);


MainUI::MainUI()
    : m_view_index(0),
      m_view(s_views[0])
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
        s_fontstore.registerFont(fontpath);
        progress_img.update_progress(progress);
    });

    // Join adjacent ranges that use the same font
    //
    // This massively reduces the memory footprint of the index (10-fold), at the cost of not
    // being able to identify missing codepoints with the index. This is fine as we can ask
    // the associated font before rendering if it actually has a glyph.
    s_fontstore.optimise();

    // Show the full logo briefly before switching to the application
    sleep_ms(250);

    // Clear logo from screen to make room for the application
    erase_rect.blank_and_invalidate();
    st7789_deselect();
}

void MainUI::tick()
{
    m_view->tick();
}

void MainUI::render()
{
    m_view->render();
}

void MainUI::set_low_byte(uint8_t value)
{
    m_view->set_low_byte(value);
}

void MainUI::shift()
{
    m_view->shift();
}

void MainUI::toggle_shift_lock()
{
    m_view->toggle_shift_lock();
}

void MainUI::reset()
{
    m_view->reset();
}

void MainUI::flush_buffer()
{
    m_view->flush_buffer();
}

const std::vector<uint32_t> MainUI::get_codepoints()
{
    return m_view->get_codepoints();
}

void MainUI::goto_next_mode(uint8_t input_switches)
{
    // Forward to the view if it has another mode to show
    if (m_view->goto_next_mode()) {
        // View handled the mode change internally
        return;
    }

    // Replace with the next available view
    m_view_index++;
    if (m_view_index >= s_num_views) {
        m_view_index = 0;
    }

    UIDelegate* last_view = m_view;
    m_view = s_views[m_view_index];

    last_view->clear();

    // Handle transition to UTF8View
    {
        UIDelegate* utf8view = dynamic_cast<UTF8View*>(m_view);
        if (utf8view != nullptr) {
            const uint32_t codepoint = last_view->get_codepoints()[0];
            const char* encoded = codepoint_to_utf8(codepoint);

            if (encoded != nullptr) {
                m_view->reset();

                bool first = true;
                while (*encoded != '\0') {
                    if (first) {
                        first = false;
                        m_view->reset();
                    } else {
                        m_view->shift();
                    }

                    m_view->set_low_byte(*encoded);
                    encoded++;
                }
            } else {
                // Can't be encoded as UTF-8: just take the current input from the switches
                auto buf = last_view->get_buffer();
                m_view->reset();
                m_view->set_low_byte(input_switches);
            }

            return;
        }
    }

    // Handle transition out of UTF8View
    {
        UIDelegate* utf8view = dynamic_cast<UTF8View*>(last_view);
        if (utf8view != nullptr) {
            const auto codepoints = last_view->get_codepoints();

            if (codepoints.size() > 0) {
                const uint32_t codepoint = codepoints.at(0);
                m_view->reset();
                m_view->set_low_byte((codepoint >> 24) & 0xFF);
                m_view->shift();
                m_view->set_low_byte((codepoint >> 16) & 0xFF);
                m_view->shift();
                m_view->set_low_byte((codepoint >> 8) & 0xFF);
                m_view->shift();
                m_view->set_low_byte(codepoint & 0xFF);

                // Converted back to codepoint and passed to next view
                return;                
            } else {
                // Can't be encoded as UTF-8: fall through to copying the buffer below
            }
        }
    }

    // Copy buffer as-is
    bool first = true;
    for (uint8_t byte : last_view->get_buffer()) {
        if (first) {
            first = false;
            m_view->reset();
        } else {
            m_view->shift();
        }

        m_view->set_low_byte(byte);
    }
}
