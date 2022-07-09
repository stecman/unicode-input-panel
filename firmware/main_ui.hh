#pragma once
#include "font.hh"

class MainUI {
public:

    MainUI();

    void run_demo();

    void show_codepoint(uint32_t codepoint);

private:
    FontStore m_fontstore;

    UIRect m_last_draw;
};