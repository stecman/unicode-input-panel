#pragma once

#include "common.hh"
#include "font.hh"

class MainUI {
public:

    enum DisplayMode {
        kMode_Hex = 0,
        kMode_Decimal,
        kMode_END,
    };

    MainUI();

    /**
     * Update time-based parts of the application and rendering as needed
     * This should be called at ~30 Hz by the host system to keep the UI fluid
     */
    void tick();

    /**
     * Immediately draw any items that need rendering to the screen
     * This is called automatically by tick() and shouldn't need to be called manually.
     */
    void render();

    /**
     * Update the low byte of the current buffer
     * This is for passing the state of input switches through to the application
     */
    void set_low_byte(uint8_t mask);

    /**
     * Shift the current buffer one byte to the left to begin input on the next byte
     */
    void shift();

    /**
     * Enable shift-lock
     * This holds all high bytes of the buffer across send operations
     */
    void set_shift_lock(bool enable);

    /**
     * Change to the next available display mode
     */
    void goto_next_mode();

    /**
     * Check if shift-lock is currently enabled
     */
    bool get_shift_lock();

    /**
     * Perform a manual reset/clear
     * This clears shift-lock and resets the buffer to be one input byte
     */
    void reset();

    /**
     * Handle the current buffer being sent
     */
    void flush_buffer();

    /**
     * Manually set the codepoint in the buffer
     */
    void set_codepoint(uint32_t codepoint);

    /**
     * Read the codepoint currently in the buffer
     */
    uint32_t get_codepoint();

private:

    void render_codepoint();
    void render_scrolling_labels();

private: // App state

    uint32_t m_codepoint;
    DisplayMode m_mode;
    bool m_shift_lock;

    bool m_codepoint_dirty;

private: // Rendering state
    FontStore m_fontstore;

    ScrollingLabel m_block_label;
    ScrollingLabel m_codepoint_label;

    UIRect m_last_draw;
    UIRect m_title_draw;
};
