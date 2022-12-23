#pragma once

#include "ui/common.hh"
#include "ui/font.hh"
#include "ui/main_ui.hh"

class CodepointView : public UIDelegate
{
public:
    CodepointView(FontStore& fontstore);

    // Implementation of UIDelegate
    // See MainUI for doc comments
    void render() override;
    bool goto_next_mode() override;
    void set_low_byte(uint8_t mask) override;
    void shift() override;
    void toggle_shift_lock() override;
    void reset() override;
    void flush_buffer() override;
    const std::vector<uint32_t> get_codepoints() override;

private:

    void render_glyph();
    void render_scrolling_labels();
    void render_input_feedback();

private: // View state

    enum DisplayMode {
        kMode_Hex = 0,
        kMode_Dec,
        kMode_END
    };

    uint32_t m_codepoint = 0;
    uint32_t m_last_codepoint = 0xFFFFFFFF;

    bool m_shift_lock = false;
    bool m_dirty = true;

    DisplayMode m_mode = DisplayMode::kMode_Hex;

private: // Drawing state

    ScrollingLabel m_block_label;
    ScrollingLabel m_codepoint_label;

    UIRect m_last_draw;
    UIRect m_title_draw;

    FontStore& m_fontstore;
};