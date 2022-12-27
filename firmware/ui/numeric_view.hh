#pragma once

#include "ui/common.hh"
#include "ui/font.hh"
#include "ui/main_ui.hh"

/**
 * Large hex value display / programmer's hex value helper
 * Sends the displayed hex string rather than the codepoint for that value
 */
class NumericView : public UIDelegate
{
public:
    NumericView(FontStore& fontstore);

    // Implementation of UIDelegate
    // See MainUI for doc comments
    void render() override;
    void set_low_byte(uint8_t value) override;
    void shift() override;
    void set_shift_lock(bool enabled) override;
    void reset() override;
    void flush_buffer() override;
    const std::vector<uint32_t> get_codepoints() override;
    std::vector<uint8_t> get_buffer() override;
    void clear() override;

private:
    void render_value();
    void render_mode_bar();

private: // View state

    uint32_t m_value = 0;

    UIRect m_last_draw;
    UIRect m_mode_bar_draw;

    FontStore& m_fontstore;

    bool m_shift_lock = false;
    bool m_dirty = true;
};