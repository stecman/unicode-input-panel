#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp/board.h"

void usb_init(void);
void usb_poll(void);

void usb_set_key_report(uint8_t report[6], uint8_t modifiers);
bool usb_last_report_sent(void);

#ifdef __cplusplus
}
#endif