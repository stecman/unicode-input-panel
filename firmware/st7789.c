/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * https://github.com/ArmDeveloperEcosystem/st7789-library-for-pico
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include "st7789.h"

#include <string.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"

#define ST7789_NOP 0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID 0x04
#define ST7789_RDDST 0x09

#define ST7789_SLPIN 0x10
#define ST7789_SLPOUT 0x11
#define ST7789_PTLON 0x12
#define ST7789_NORON 0x13

#define ST7789_INVOFF 0x20
#define ST7789_INVON 0x21
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON 0x29

#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_RAMRD 0x2E

#define ST7789_PTLAR 0x30
#define ST7789_MADCTL 0x36
#define ST7789_COLMOD 0x3A

#define ST7789_FRMCTR1 0xB1
#define ST7789_FRMCTR2 0xB2
#define ST7789_FRMCTR3 0xB3
#define ST7789_INVCTR 0xB4
#define ST7789_DISSET5 0xB6

#define ST7789_GCTRL 0xB7
#define ST7789_GTADJ 0xB8
#define ST7789_VCOMS 0xBB

#define ST7789_LCMCTRL 0xC0
#define ST7789_IDSET 0xC1
#define ST7789_VDVVRHEN 0xC2
#define ST7789_VRHS 0xC3
#define ST7789_VDVS 0xC4
#define ST7789_VMCTR1 0xC5
#define ST7789_FRCTRL2 0xC6
#define ST7789_CABCCTRL 0xC7

#define ST7789_RDID1 0xDA
#define ST7789_RDID2 0xDB
#define ST7789_RDID3 0xDC
#define ST7789_RDID4 0xDD

#define ST7789_GMCTRP1 0xE0
#define ST7789_GMCTRN1 0xE1

#define ST7789_PWCTR6 0xFC

#ifdef __cplusplus
extern "C" {
#endif

static struct st7789_config st7789_cfg;
static uint16_t st7789_width;
static uint16_t st7789_height;

static bool is_writing_pixels = false;

static int dma_ch = -1;
static dma_channel_config tx_dma_cfg;

static void wait_for_dma()
{
    dma_channel_wait_for_finish_blocking(dma_ch);

    spi_inst_t* spi = st7789_cfg.spi;

    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while (spi_is_readable(spi)) {
        (void) spi_get_hw(spi)->dr;
    }
    while (spi_get_hw(spi)->sr & SPI_SSPSR_BSY_BITS) {
        tight_loop_contents();
    }
    while (spi_is_readable(spi)) {
        (void) spi_get_hw(spi)->dr;
    }

    // Don't leave overrun flag set
    spi_get_hw(spi)->icr = SPI_SSPICR_RORIC_BITS;
}

static void st7789_cmd(uint8_t cmd, const uint8_t* data, size_t len)
{
    if (is_writing_pixels) {
        wait_for_dma();
    }

    is_writing_pixels = false;

    gpio_put(st7789_cfg.gpio_cs, 0);
    gpio_put(st7789_cfg.gpio_dc, 0);

    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));

    if (len) {
        gpio_put(st7789_cfg.gpio_dc, 1);
        spi_write_blocking(st7789_cfg.spi, data, len);
    }

    gpio_put(st7789_cfg.gpio_cs, 1);
}

static void st7789_cmd_one_parm(uint8_t cmd, uint8_t param)
{
    st7789_cmd(cmd, &param, 1);
}

void st7789_caset(uint16_t xs, uint16_t xe)
{
    const uint8_t data[] = {
        xs >> 8,
        xs & 0xff,
        xe >> 8,
        xe & 0xff,
    };

    // CASET (2Ah): Column Address Set
    st7789_cmd(ST7789_CASET, data, sizeof(data));
}

void st7789_raset(uint16_t ys, uint16_t ye)
{
    const uint8_t data[] = {
        ys >> 8,
        ys & 0xff,
        ye >> 8,
        ye & 0xff,
    };

    // RASET (2Bh): Row Address Set
    st7789_cmd(ST7789_RASET, data, sizeof(data));
}

void st7789_init(const struct st7789_config* config, uint16_t width, uint16_t height)
{
    memcpy(&st7789_cfg, config, sizeof(st7789_cfg));
    st7789_width = width;
    st7789_height = height;

    spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(st7789_cfg.gpio_din, GPIO_FUNC_SPI);
    gpio_set_function(st7789_cfg.gpio_clk, GPIO_FUNC_SPI);

    gpio_init(st7789_cfg.gpio_cs);
    gpio_init(st7789_cfg.gpio_dc);
    gpio_init(st7789_cfg.gpio_rst);
    // gpio_init(st7789_cfg.gpio_bl);

    gpio_set_dir(st7789_cfg.gpio_cs, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_dc, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_rst, GPIO_OUT);
    // gpio_set_dir(st7789_cfg.gpio_bl, GPIO_OUT);

    gpio_put(st7789_cfg.gpio_cs, 1);
    gpio_put(st7789_cfg.gpio_dc, 0);

    // Perform hardware reset on the display
    gpio_put(st7789_cfg.gpio_rst, 0);
    sleep_ms(5);
    gpio_put(st7789_cfg.gpio_rst, 1);
    sleep_ms(5);

    // SWRESET (01h): Software Reset
    st7789_cmd(ST7789_SWRESET, NULL, 0);
    sleep_ms(150);

    // SLPOUT (11h): Sleep Out
    st7789_cmd(ST7789_SLPOUT, NULL, 0);
    sleep_ms(255);

    // COLMOD (3Ah): Interface Pixel Format
    // - RGB interface color format     = 65K of RGB interface
    // - Control interface color format = 18bit/pixel
    st7789_cmd_one_parm(ST7789_COLMOD, 0x67);
    sleep_ms(10);

    // MADCTL (36h): Memory Data Access Control
    // - Page Address Order            = Top to Bottom
    // - Column Address Order          = Left to Right
    // - Page/Column Order             = Normal Mode
    // - Line Address Order            = LCD Refresh Top to Bottom
    // - RGB/BGR Order                 = RGB
    // - Display Data Latch Data Order = LCD Refresh Left to Right
    st7789_cmd_one_parm(ST7789_MADCTL, 0x00);

    // Set display extents in the LCD's memory
    st7789_caset(0, width);
    st7789_raset(0, height);

    // INVON (21h): Display Inversion On
    st7789_cmd(ST7789_INVON, NULL, 0);
    sleep_ms(10);

    // NORON (13h): Normal Display Mode On
    st7789_cmd(ST7789_NORON, NULL, 0);
    sleep_ms(10);


    // Set up DMA channel
    dma_ch = dma_claim_unused_channel(true);

    tx_dma_cfg = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&tx_dma_cfg, DMA_SIZE_8);
    channel_config_set_write_increment(&tx_dma_cfg, false);

    // Drive the transfer using the SPI TX's signal.
    channel_config_set_dreq(&tx_dma_cfg, spi_get_index(st7789_cfg.spi) ? DREQ_SPI1_TX : DREQ_SPI0_TX);
}

void st7789_display_on(bool display_on)
{
    if (display_on) {
        st7789_cmd(ST7789_DISPON, NULL, 0);
    } else {
        st7789_cmd(ST7789_DISPOFF, NULL, 0);
    }
}

void st7789_ramwr()
{
    gpio_put(st7789_cfg.gpio_cs, 0);
    gpio_put(st7789_cfg.gpio_dc, 0);

    // RAMWR (2Ch): Memory Write
    const uint8_t cmd = ST7789_RAMWR;
    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));
    is_writing_pixels = true;

    gpio_put(st7789_cfg.gpio_cs, 0);
    gpio_put(st7789_cfg.gpio_dc, 1);
}

void st7789_write(const void* data, size_t len)
{
    if (!is_writing_pixels) {
        st7789_ramwr();
    }

    spi_write_blocking(st7789_cfg.spi, data, len);
}

void st7789_put(uint32_t pixel)
{
    pixel << 8; // Shift into the first 3 bytes
    st7789_write(&pixel, 3);
}

void st7789_put_mono(uint8_t pixel)
{
    uint8_t buf[3];

    buf[0] = pixel;
    buf[1] = pixel;
    buf[2] = pixel;

    st7789_write((uint8_t*) &buf, sizeof(buf));
}

void st7789_write_dma(const void* data, size_t len, bool increment)
{
    wait_for_dma();

    gpio_put(st7789_cfg.gpio_cs, 0);

    // Prepare for writing pixel data
    if (!is_writing_pixels) {
        st7789_ramwr();
    }

    channel_config_set_read_increment(&tx_dma_cfg, increment);
    dma_channel_configure(
        dma_ch,
        &tx_dma_cfg,
        &spi_get_hw(st7789_cfg.spi)->dr,
        data,
        len,
        true
    );

    // gpio_put(st7789_cfg.gpio_cs, 1);
}

static uint8_t g_fill_value;

void st7789_fill(uint8_t pixel)
{
    const uint num_bytes = st7789_width * st7789_height * 3;

    g_fill_value = pixel;

    st7789_set_cursor(0, 0);
    st7789_write_dma(&g_fill_value, num_bytes, false);
}

void st7789_fill_window(uint8_t pixel, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    g_fill_value = pixel;

    st7789_set_window(x, y, x + width, y + height);
    st7789_write_dma(&g_fill_value, width * height * 3, false);
}

void st7789_fill_colour(uint32_t pixel)
{
    uint num_pixels = st7789_width * st7789_height;

    st7789_set_cursor(0, 0);

    while (num_pixels-- != 0) {
        st7789_put(pixel);
    }

    gpio_put(st7789_cfg.gpio_cs, 1);
}

void st7789_deselect(void)
{
    wait_for_dma();
    gpio_put(st7789_cfg.gpio_cs, 1);
}

void st7789_set_cursor(uint16_t x, uint16_t y)
{
    st7789_caset(x, st7789_width - 1);
    st7789_raset(y, st7789_height - 1);
}

void st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    st7789_caset(x1, x2 - 1);
    st7789_raset(y1, y2 - 1);
}

void st7789_vertical_scroll(uint16_t row)
{
    uint8_t data[] = {
        (row >> 8) & 0xff,
        row & 0x00ff
    };

    // VSCSAD (37h): Vertical Scroll Start Address of RAM
    st7789_cmd(0x37, data, sizeof(data));
}


uint8_t* st7789_line_buffer(void)
{
    static uint8_t active = 1;
    static uint8_t buffer_a[ST7789_LINE_BUF_SIZE];
    static uint8_t buffer_b[ST7789_LINE_BUF_SIZE];

    active = !active;
    return active == 0 ? (uint8_t*) &buffer_a : (uint8_t*) &buffer_b;
}

#ifdef __cplusplus
}
#endif