/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "usb.h"
#include "tusb.h"
#include "usb_descriptors.h"

static uint8_t s_key_report[6] = {0};
static uint8_t s_key_modifiers = 0;

static bool s_report_required = true;
static bool s_has_sent_report = false;

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

void tud_mount_cb(void)
{
    // Invoked when device is mounted
}

void tud_umount_cb(void)
{
    // Invoked when device is unmounted
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    // Invoked when usb bus is suspended
    // remote_wakeup_en : if host allow us  to perform remote wakeup
    // Within 7ms, device must draw an average of current less than 2.5 mA from bus
}

void tud_resume_cb(void)
{
    // Invoked when usb bus is resumed
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void usb_init(void)
{
    board_init();
    tusb_init();
}

void usb_poll(void)
{
    tud_task();

    // Send reports at a fixed interval
    const uint32_t interval_ms = 5;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms)
        return; // not enough time
    start_ms += interval_ms;

    if (!tud_hid_ready()) {
        return;
    }

    if (s_report_required) {
        s_report_required = false;
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, s_key_modifiers, s_key_report);
        return;
    }
}

void usb_set_key_report(uint8_t report[6], uint8_t modifiers)
{
    s_report_required = true;
    s_has_sent_report = false;

    s_key_modifiers = modifiers;
    memcpy(s_key_report, report, sizeof(s_key_report));
}

bool usb_last_report_sent(void)
{
    return s_has_sent_report;
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    s_has_sent_report = true;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // Not implemented
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    // Not implemented
    return;
}