#include "st7789.h"
#include "main_ui.hh"

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include <stdio.h>

#define PIN_UART_DEBUG_TX 0
#define PIN_DISP_CS 16
#define PIN_DISP_BKLGHT 17
#define PIN_DISP_SPI_SCK 18
#define PIN_DISP_SPI_TX 19
#define PIN_DISP_RESET 20
#define PIN_DISP_DC 21

struct st7789_config display_config ={
	.spi = spi0,
    .gpio_din = PIN_DISP_SPI_TX,
    .gpio_clk = PIN_DISP_SPI_SCK,
    .gpio_cs = PIN_DISP_CS,
    .gpio_dc = PIN_DISP_DC,
    .gpio_rst = PIN_DISP_RESET,
    .gpio_bl = PIN_DISP_BKLGHT
};

int main()
{
    stdio_uart_init_full(uart0, 115200, PIN_UART_DEBUG_TX, -1);

    printf("\n\nDevice has reset\n");

	// GPIOs 0-11 as inputs with pull-up
	// (Looping as there's no mask function for direction)
	for (unsigned int pin = 1; pin < 11; pin++) {
		gpio_init(pin);
		gpio_pull_up(pin);
		gpio_set_dir(pin, GPIO_IN);
	}

	// Configure backlight PWM
	{
		gpio_set_function(PIN_DISP_BKLGHT, GPIO_FUNC_PWM);

	    const uint slice_num = pwm_gpio_to_slice_num(PIN_DISP_BKLGHT);
	    pwm_set_wrap(slice_num, 255);
	    pwm_set_chan_level(slice_num, PWM_CHAN_A, 127);
	    pwm_set_enabled(slice_num, true);
	}

	// Select pins to use for SPI0
	gpio_set_function(PIN_DISP_SPI_SCK, GPIO_FUNC_SPI);
	gpio_set_function(PIN_DISP_SPI_TX, GPIO_FUNC_SPI);

	// Enable SPI0 for screen
	spi_init(spi0, 25e6);

    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Set up and blank display
    st7789_init(&display_config, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // The particular screen I have has Y 0-20 outside of the display by default
    // Scroll so 0,0 in memory is actually rendered in the top-left corner
    st7789_vertical_scroll(300);

    MainUI app;
    app.run_demo();

    while (true) {
  //       gpio_put(LED_PIN, !gpio_get(0));

  //       const uint8_t switches = ~((uint8_t) gpio_get_all());

		// pwm_set_gpio_level(PIN_DISP_BKLGHT, counter);
		// if (counter == 255 || counter == 0) {
		// 	direction *= -1;
		// }

		// counter += direction;

		// // fill += 64;
		// // st7789_fill(fill);

        gpio_put(LED_PIN, 1);
        sleep_ms(900);
        gpio_put(LED_PIN, 0);
        sleep_ms(900);
    }
}
