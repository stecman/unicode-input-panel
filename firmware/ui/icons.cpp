#include "icons.hh"
#include "st7789.h"
#include "embeds.hh"

// C
#include <stdlib.h>
#include <string.h>


static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    memcpy(data, png_ptr->io_ptr, length);
    png_ptr->io_ptr = ((uint8_t*) png_ptr->io_ptr) + length;
}

PngImage::PngImage(const uint8_t* buffer)
{
    // Check if the first 8 bytes actually look like a PNG file
    if (png_sig_cmp(buffer, 0, 8) != 0) {
        printf("Failed PNG header test\n");
        png_ptr = nullptr;
        info_ptr = nullptr;
        return;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL) {
        printf("[read_png_file] png_create_read_struct failed\n");
        png_ptr = nullptr;
        info_ptr = nullptr;
        return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        printf("[read_png_file] png_create_info_struct failed\n");
        png_ptr = nullptr;
        info_ptr = nullptr;
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        printf("[read_png_file] Error during io\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        png_ptr = nullptr;
        info_ptr = nullptr;
        return;
    }

    png_ptr->io_ptr = const_cast<uint8_t*>(buffer);
    png_set_read_fn(png_ptr, const_cast<uint8_t*>(buffer), user_read_data);
    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(
        png_ptr, info_ptr,
        &width, &height,
        &bit_depth, &color_type, &interlace_type,
        NULL, NULL
    );

    // printf("%d x %d @ %d bpp. Colour = %d, interlace = %d\n", width, height, bit_depth, color_type, interlace_type);

    // Ensure the image is transformed to RGB on retrieval if it's stored in a different format
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    // Ensure we get 8-bpp when reading the image data out (truncate or expand as needed)
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    } else if (bit_depth < 8) {
        png_set_packing(png_ptr);
    }

    // Remove alpha channel as we only want RGB pixel data
    png_color_16 black = {0, 0, 0, 0, 0};
    png_set_background(png_ptr, &black, PNG_BACKGROUND_GAMMA_SCREEN, 0 /*do not expand*/, 1 /* gamma */);
    png_set_strip_alpha(png_ptr);
}

PngImage::~PngImage()
{
    if (png_ptr != nullptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
}

void PngImage::draw(uint16_t origin_x, uint16_t origin_y)
{
    if (png_ptr == nullptr) {
        return;
    }

    const uint16_t end_x = origin_x + width;
    const uint16_t end_y = origin_y + height;

    st7789_set_window(origin_x, origin_y, end_x, end_y);

    for (uint16_t y = origin_y; y < end_y; y++) {
        uint8_t* buf = st7789_line_buffer();
        png_read_row(png_ptr, (png_bytep) buf, NULL);
        st7789_write_dma(buf, width * 3, true);
    }

    png_read_end(png_ptr, NULL);
}



ProgressPngImage::ProgressPngImage(const uint8_t* buffer)
    : m_imgbuffer(buffer),
      m_png(nullptr),
      m_last_fill_width(0),
      m_x(0),
      m_y(0),
      m_effect_y_min(0),
      m_effect_y_max(0) {}

ProgressPngImage::ProgressPngImage(const uint8_t* buffer, uint16_t effect_y_min, uint16_t effect_y_max)
    : m_imgbuffer(buffer),
      m_png(nullptr),
      m_last_fill_width(0),
      m_x(0),
      m_y(0),
      m_effect_y_min(effect_y_min),
      m_effect_y_max(effect_y_max) {}

PngImage* ProgressPngImage::load()
{
    if (m_png == nullptr) {
        m_png = new PngImage(m_imgbuffer);
    }

    return m_png;
}

void ProgressPngImage::draw_initial(uint16_t origin_x, uint16_t origin_y, uint8_t progress)
{
    load();

    m_x = origin_x;
    m_y = origin_y;

    if (m_effect_y_max == 0) {
        m_effect_y_max = m_png->height;
    }

    draw_full(m_x, m_y, progress);

    delete m_png;
    m_png = nullptr;
}

void ProgressPngImage::update_progress(uint8_t progress)
{
    load();

    draw_update(m_x, m_y, progress);

    delete m_png;
    m_png = nullptr;
}

static uint16_t calculate_fill_width(uint16_t progress, uint16_t width)
{
    return (width * (progress + 1)) >> 8;
}

void ProgressPngImage::draw_full(uint16_t origin_x, uint16_t origin_y, uint8_t progress)
{
    const uint16_t fill_width = calculate_fill_width(progress, m_png->width);

    // Draw the whole image
    const uint16_t end_x = origin_x + m_png->width;
    const uint16_t end_y = origin_y + m_effect_y_max;

    st7789_set_window(origin_x, origin_y, end_x, end_y);

    for (uint16_t y = 0; y < m_png->height; y++) {
        uint8_t* buf = st7789_line_buffer();
        png_read_row(m_png->png_ptr, (png_bytep) buf, NULL);

        // Desaturate the "incomplete" portion of the image
        if (y >= m_effect_y_min && y < m_effect_y_max) {
            for (uint16_t x = fill_width; x < m_png->width; x++) {
                uint8_t* pixel = buf + (x * 3);
                desaturate(pixel);
            }
        }

        st7789_write_dma(buf, m_png->width * 3, true);
    }

    png_read_end(m_png->png_ptr, NULL);

    m_last_fill_width = fill_width;
}

void ProgressPngImage::draw_update(uint16_t origin_x, uint16_t origin_y, uint8_t progress)
{
    const uint16_t fill_width = calculate_fill_width(progress, m_png->width);
    const int16_t slice_width = fill_width - m_last_fill_width;

    if (slice_width == 0) {
        // Nothing needs updating
        return;

    } else if (slice_width < 0) {
        // Loss of progress isn't implemented as an incremental update
        draw_full(origin_x, origin_y, progress);
        return;
    }

    const uint16_t start_x = origin_x + m_last_fill_width;
    const uint16_t end_x = origin_x + fill_width;

    st7789_set_window(start_x, origin_y + m_effect_y_min, end_x, origin_y + m_effect_y_max);

    for (uint16_t y = 0; y < m_effect_y_max; y++) {
        uint8_t* buf = st7789_line_buffer();
        png_read_row(m_png->png_ptr, (png_bytep) buf, NULL);

        if (y >= m_effect_y_min && y < m_effect_y_max) {
            st7789_write_dma(buf + (m_last_fill_width * 3), slice_width * 3, true);
        }
    }

    png_read_end(m_png->png_ptr, NULL);

    m_last_fill_width = fill_width;
}

void ProgressPngImage::desaturate(uint8_t* pixel)
{
    // Crudely work out greyscale conversion - we don't really care about accuracy
    const uint16_t luma = ((pixel[0] << 3) + (pixel[1] << 6) + (pixel[3] << 1)) >> 6;
    const uint8_t new_value = luma > 255 ? 255 : luma;

    // Change in-place
    pixel[0] = new_value;
    pixel[1] = new_value;
    pixel[2] = new_value;
}


ProgressPngImage icons_unicode_logo()
{
    return ProgressPngImage(assets::unicode_logo_png);
}


#if PICO_ON_DEVICE
// This isn't actually used currently - just here for reference

#include "f_util.h"
#include "ff.h"

static void draw_from_file(const char* path, FIL* fp)
{
    FRESULT fr;
    unsigned int bytes_read = 0;

    const auto size = f_size(fp);
    if (size > 15360) {
        printf("Image file is too large: refusing to open with libpng\n");
        return;
    }

    uint8_t* buffer = (uint8_t*) malloc(size);
    if (buffer == NULL) {
        printf("Failed to malloc %lu bytes to load image\n", size);
        return;
    }

    uint8_t* cursor = buffer;
    while (!f_eof(fp)) {
        fr = f_read(fp, cursor, 2048, &bytes_read);
        if (fr != FR_OK) {
            printf("Failed to read image from SD card: %s (%d)\n", FRESULT_str(fr), fr);
            free(buffer);
            return;
        }

        cursor += bytes_read;
    }

    PngImage image(assets::unicode_logo_png);
    image.draw(
        (DISPLAY_WIDTH - image.width) / 2,
        (DISPLAY_HEIGHT - image.height) / 2
    );

    free(buffer);
}
#endif
