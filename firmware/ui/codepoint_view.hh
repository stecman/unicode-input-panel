#pragma once

#include "ui/common.hh"
#include "ui/font.hh"
#include "ui/glyph_display.hh"
#include "ui/main_ui.hh"

class CodepointView : public UIDelegate
{
public:
    CodepointView(FontStore& fontstore);

    // Implementation of UIDelegate
    // See MainUI for doc comments
    void render() override;
    bool goto_next_mode() override;
    void set_low_byte(uint8_t value) override;
    void shift() override;
    void set_shift_lock(bool enabled) override;
    void reset() override;
    void flush_buffer() override;
    const std::vector<uint32_t> get_codepoints() override;
    std::vector<uint8_t> get_buffer() override;
    void clear() override;

private:
    void render_input_feedback();

private: // View state

    enum DisplayMode {
        kMode_Hex = 0,
        kMode_Dec,
        kMode_END
    };

    uint32_t m_codepoint = 0;
    uint32_t m_last_codepoint = kInvalidEncoding;

    bool m_shift_lock = false;
    bool m_dirty = true;

    DisplayMode m_mode = DisplayMode::kMode_Hex;

private: // Drawing state

    CodepointTitle m_title_display;
    GlyphDisplay m_glyph_display;

    UIRect m_last_draw;
    UIRect m_mode_bar_draw;
    UIRect m_codepoint_value_draw;

    FontStore& m_fontstore;
};