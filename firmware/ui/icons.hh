#pragma once

#include <png.h>
#include <pngstruct.h>

#include <stdint.h>


class PngImage {
public:
    PngImage(const uint8_t* buffer);
    ~PngImage();

    void draw(uint16_t origin_x, uint16_t origin_y);

    inline bool is_valid()
    {
        return png_ptr != nullptr;
    }

public:
    png_structp png_ptr;
    png_infop info_ptr;

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
};


/**
 * PNG that is desaturated then coloured in to indicate progress
 */
class ProgressPngImage {
public:

    ProgressPngImage(const uint8_t* buffer);
    ProgressPngImage(const uint8_t* buffer, uint16_t effect_y_min, uint16_t effect_y_max);

    // Load the PNG image if it hasn't been loaded yet
    // The returned pointer is invalid after draw_progress is called
    PngImage* load();

    void draw_initial(uint16_t origin_x, uint16_t origin_y, uint8_t progress = 0x0);
    void update_progress(uint8_t progress);

private:

    void draw_full(uint16_t origin_x, uint16_t origin_y, uint8_t progress);
    void draw_update(uint16_t origin_x, uint16_t origin_y, uint8_t progress);
    void desaturate(uint8_t* pixel);

private:
    const uint8_t* m_imgbuffer;
    PngImage* m_png;

    int16_t m_last_fill_width;

    uint16_t m_x;
    uint16_t m_y;

    uint16_t m_effect_y_min;
    uint16_t m_effect_y_max;
};

ProgressPngImage icons_unicode_logo();