#pragma once

#include "ui/common.hh"
#include "ui/font.hh"
#include "ui/main_ui.hh"

#pragma once

#include "ui/common.hh"
#include "ui/font.hh"
#include "ui/glyph_display.hh"
#include "ui/main_ui.hh"

class UTF8View : public UIDelegate
{
public:
    UTF8View(FontStore& fontstore);

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
    void render_large_input_help();
    void render_small_input_help();
    void render_invalid_banner();
    void render_byte(UIFontPen &pen, uint index, char* str, uint16_t text_width, UIRect &painted);
    void render_mode_bar();

private: // View state
    uint8_t m_buffer[4];
    uint8_t m_index;

    bool m_shift_lock = false;
    bool m_dirty = true;

private: // Drawing state

    CodepointTitle m_title_display;
    GlyphDisplay m_glyph_display;

    UIRect m_invalid_encoding;
    UIRect m_codepoint_value_draw;
    UIRect m_small_help;
    UIRect m_large_help;
    UIRect m_mode_bar_draw;

    uint32_t m_last_length;

    FontStore& m_fontstore;
};
