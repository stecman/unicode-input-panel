#pragma once
#include "font.hh"


class ScrollingLabel {
public:

    ScrollingLabel();

    ScrollingLabel(const char* text, int y = 0, int padding = 0);

    /**
     * Set the label contents to null and blank the last drawn region
     */
    void clear();

    /**
     * Change the text and reset the scroll of this label
     */
    void replace(const char* text);

    /**
     * Draw the next frame of the label to screen
     */
    void render(UIFontPen &pen);

    inline const char* value()
    {
        return m_str;
    }

private:

    enum AnimationState {
        kState_New,
        kState_Fixed,
        kState_WaitingLeft,
        kState_Animating,
        kState_WaitingRight,
        kState_AnimatingReset,
    };

    const char* m_str;
    int m_y;
    int m_padding;

    int16_t m_x;
    int16_t m_start_x;
    int16_t m_end_x;
    int16_t m_width;

    uint m_tick;
    uint m_next_tick;
    AnimationState m_state;


    UIRect m_last_draw;
};


class MainUI {
public:

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
    bool m_shift_lock;

    bool m_codepoint_dirty;

private: // Rendering state
    FontStore m_fontstore;

    ScrollingLabel m_block_label;
    ScrollingLabel m_codepoint_label;

    UIRect m_last_draw;
};
