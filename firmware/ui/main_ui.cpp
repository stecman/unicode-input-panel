#include "main_ui.hh"

#include "filesystem.hh"
#include "st7789.h"
#include "ui/icons.hh"
#include "ui/codepoint_view.hh"

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

void MainUI::goto_next_mode()
{
    // Forward to the view if it has another mode to show
    if (m_view->goto_next_mode()) {
        // View handled the mode change internally
        return;
    }

    m_view_index++;
    if (m_view_index >= s_num_views) {
        m_view_index = 0;
    }

    m_view = s_views[m_view_index];
}