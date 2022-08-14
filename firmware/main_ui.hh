#pragma once
#include "font.hh"


class ScrollingLabel {
public:

    ScrollingLabel();

    ScrollingLabel(const char* text, int y = 0, int padding = 0);

    void render(UIFontPen &pen);

    void clear();

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

    void run_demo();

    void set_codepoint(uint32_t codepoint);

    void update();

private:
    FontStore m_fontstore;

    ScrollingLabel m_block_label;
    ScrollingLabel m_codepoint_label;

    UIRect m_last_draw;
};