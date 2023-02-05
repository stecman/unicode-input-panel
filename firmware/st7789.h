/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * https://github.com/ArmDeveloperEcosystem/st7789-library-for-pico
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#pragma once

#include "defs.h"

#include <stdint.h>
#include <stdbool.h>
#include <memory.h>

#ifdef __cplusplus
extern "C" {
#endif

#if PICO_ON_DEVICE
#include "hardware/spi.h"

struct st7789_config {
    spi_inst_t* spi;
    uint32_t gpio_din;
    uint32_t gpio_clk;
    int gpio_cs;
    uint32_t gpio_dc;
    uint32_t gpio_rst;
    uint32_t gpio_bl;
};

void st7789_init(const struct st7789_config* config, uint16_t width, uint16_t height);
#else
void st7789_init(uint32_t** buffer, uint16_t width, uint16_t height);
#endif

#define ST7789_LINE_LEN_PX DISPLAY_WIDTH
#define ST7789_LINE_BUF_SIZE (ST7789_LINE_LEN_PX*3)


void st7789_display_on(bool display_on);
void st7789_write(const void* data, size_t len);
void st7789_write_dma(const void* data, size_t len, bool increment);
void st7789_put(uint32_t pixel);
void st7789_put_mono(uint8_t pixel);
void st7789_fill(uint8_t pixel);
void st7789_fill_window(uint8_t pixel, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void st7789_fill_window_colour(uint32_t pixel, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void st7789_fill_colour(uint32_t pixel);
void st7789_set_cursor(uint16_t x, uint16_t y);
void st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void st7789_vertical_scroll(uint16_t row);

/**
 * Return the least recently used of two internal line buffers
 * These are intended for the caller to fill and pass to st7789_write_dma.
 *
 * The buffer contents is not zeroed, so the caller must do this if required.
 * @return A writable buffer of ST7789_LINE_BUF_SIZE bytes (ST7789_LINE_LEN_PX RGB pixels)
 */
uint8_t* st7789_line_buffer(void);

void st7789_deselect(void);

#ifdef __cplusplus
}
#endif
