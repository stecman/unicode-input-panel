#pragma once

#include "common.hh"
#include "font.hh"

class UIDelegate {
public:
    // Forwarded methods from MainUI
    // See MainUI for doc comments
    virtual void tick() { render(); }
    virtual void render() = 0;
    virtual void set_low_byte(uint8_t value) = 0;
    virtual void shift() = 0;
    virtual void set_shift_lock(bool enabled) = 0;
    virtual void reset() = 0;
    virtual void flush_buffer() = 0;
    virtual const std::vector<uint32_t> get_codepoints() = 0;

    /**
     * Get the underlying data being manipulated by the input switches
     */
    virtual std::vector<uint8_t> get_buffer() = 0;

    /**
     * Go to the next available display mode in this view
     * Returns false if all available display modes have been exhausted.
     */
    virtual bool goto_next_mode()
    {
        // No modes by default
        return false;
    }

    /**
     * Check if this delegate works in UTF8 instead of codepoint value
     * This is required to work around the Pico SDK disabling RTTI (no dynamic_cast or typeid)
     */
    virtual inline bool uses_utf8()
    {
        return false;
    }

    /**
     * Clear screen, making it ready for another view
     */
    virtual void clear() = 0;
};

class MainUI {
public:

    MainUI(const char* fontdir);

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
     * Update the least significant byte of the current buffer
     * This is for passing the state of input switches through to the application
     */
    void set_low_byte(uint8_t value);

    /**
     * Shift the current buffer one byte to the left to begin input on the next byte
     */
    void shift();

    /**
     * Toggle shift-lock
     * The exact effect differs between views, but this generally holds the high bytes
     * of the buffer across send operations instead of clearing them.
     */
    void toggle_shift_lock();

    /**
     * Change to the next available display mode
     */
    void goto_next_mode(uint8_t input_switches);

    /**
     * Perform a manual reset/clear
     * This clears shift-lock and resets the buffer to the current low byte
     */
    void reset();

    /**
     * Handle the current buffer being sent
     */
    void flush_buffer();

    /**
     * Read the current buffer as codepoints to output
     * This may be empty if the current view doesn't have any valid codepoint available
     */
    const std::vector<uint32_t> get_codepoints();

private:
    UIDelegate* m_view;
    size_t m_view_index;
    bool m_shift_lock;
};
