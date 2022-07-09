#include "st7789.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#ifdef __cplusplus
extern "C" {
#endif

static uint16_t s_cursor_x = 0;
static uint16_t s_cursor_y = 0;
static uint16_t s_width;
static uint16_t s_height;

// Reference to the allocated pixel buffer
uint32_t* s_px_buffer = NULL;

// Display update window
uint16_t s_win_x1, s_win_y1, s_win_x2, s_win_y2;

volatile bool _dirty = true;

static void cursor_advance()
{
    s_cursor_x++;

    if (s_cursor_x >= s_win_x2) {
        s_cursor_x = s_win_x1;
        s_cursor_y++;
    }

    if (s_cursor_y >= s_win_y2) {
        s_cursor_y = s_win_y1;
    }
}

static void cursor_put(uint32_t rgb)
{
    if (s_px_buffer == NULL) {
        printf("Pixel buffer has not been initialised. Was st7789_init called?\n");
        abort();
    }

    // Set RGBA pixel in buffer
    s_px_buffer[s_cursor_x + s_cursor_y * s_width] = (rgb << 8) | 0xFF;
    _dirty = true;
    cursor_advance();

    // Uncomment to slow rendering down
    // static struct timespec sleep_time = { .tv_sec = 0, .tv_nsec = 100 };
    // nanosleep(&sleep_time, NULL);
}

static uint32_t to_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) | (g << 8) | b;
}

void st7789_init(uint32_t** buffer, uint16_t width, uint16_t height)
{
    // Allocate pixel buffer
    *buffer = malloc(width * height * 4);
    s_px_buffer = *buffer;

    // Init paint cursor
    s_width = width;
    s_height = height;
    st7789_set_cursor(0, 0);
}

// All of these calls are no-ops when running on the host
void st7789_display_on(bool display_on) {}
void st7789_vertical_scroll(uint16_t row) {}
void st7789_deselect(void) {}

void st7789_write(const void* data, size_t len)
{
    st7789_write_dma(data, len, true);
}

void st7789_write_dma(const void* data, size_t len, bool increment)
{
    // Assume we're working with whole pixels
    assert((len % 3) == 0);

    const uint8_t* ptr = (const uint8_t*) data;

    if (increment) {
        for (size_t i = 0; i < len; i += 3) {
            cursor_put(to_rgb(ptr[0], ptr[1], ptr[2]));
            ptr += 3;
        }
    } else {
        const uint32_t pixel = to_rgb(ptr[0], ptr[0], ptr[0]);
        for (size_t i = 0; i < len; i += 3) {
            cursor_put(pixel);
        }
    }
}

void st7789_put(uint32_t pixel)
{
    cursor_put(pixel);
}

void st7789_put_mono(uint8_t pixel)
{
    cursor_put(to_rgb(pixel, pixel, pixel));
}

void st7789_fill(uint8_t pixel)
{
    st7789_fill_window(pixel, 0, 0, s_width, s_height);
}

void st7789_fill_window(uint8_t pixel, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    st7789_set_window(x, y, x + width, y + height);

    for (uint i = 0; i < width * height; i++) {
        cursor_put(pixel);
    }
}

void st7789_fill_colour(uint32_t pixel)
{
    st7789_set_window(0, 0, s_width, s_height);

    for (uint i = 0; i < s_width * s_height; i++) {
        cursor_put(pixel);
    }
}

void st7789_set_cursor(uint16_t x, uint16_t y)
{
    st7789_set_window(x, y, s_width, s_height);
}

void st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    s_cursor_x = x1;
    s_cursor_y = y1;

    s_win_x1 = x1;
    s_win_y1 = y1;
    s_win_x2 = x2;
    s_win_y2 = y2;

    // Catch out-of-bounds drawing
    // This will cause glitchy looking output on the display, but will usually crash this host program
    if (x1 > s_width || x2 > s_width || y1 > s_height || y2 > s_height) {
        printf("WARNING: display cursor set out of bounds: %d, %d; %d,%d\n", x1, y1, x2, y2);
    }
}

uint8_t* st7789_line_buffer(void)
{
    // Just use a single buffer as all draw calls are blocking, unlike the Pico implementation
    static uint8_t buffer[ST7789_LINE_BUF_SIZE];
    return (uint8_t*) &buffer;
}

#ifdef __cplusplus
}
#endif