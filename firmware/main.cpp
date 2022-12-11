#include "st7789.h"
#include "main_ui.hh"

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <stdio.h>

#define PIN_UART_DEBUG_TX 0
#define PIN_DISP_CS 16
#define PIN_DISP_BKLGHT 17
#define PIN_DISP_SPI_SCK 18
#define PIN_DISP_SPI_TX 19
#define PIN_DISP_RESET 20
#define PIN_DISP_DC 21

#define PIN_LED_PWM 22
#define PIN_SWITCH_SHIFT 10
#define PIN_SWITCH_MODECLEAR 11
#define PIN_SWITCH_SEND 26

struct st7789_config display_config ={
	.spi = spi0,
    .gpio_din = PIN_DISP_SPI_TX,
    .gpio_clk = PIN_DISP_SPI_SCK,
    .gpio_cs = PIN_DISP_CS,
    .gpio_dc = PIN_DISP_DC,
    .gpio_rst = PIN_DISP_RESET,
    .gpio_bl = PIN_DISP_BKLGHT
};

static volatile bool needs_render = true;
struct repeating_timer render_timer;

bool render_timer_callback(struct repeating_timer *t) {
    needs_render = true;
    return true;
}

/**
 * Get the byte selected on the data input switch panel
 */
uint8_t get_input_byte()
{
    return ((uint8_t) (gpio_get_all() >> 2));
}


/**
 * Logic for short and long presses on an active-low GPIO input
 */
class UserInput
{
public:
    UserInput(uint gpio) : m_gpio(gpio) {}

    void update()
    {
        const bool pressed = gpio_get(m_gpio) == 0;

        if (pressed != m_pressed) {
            m_pressed = pressed;

            if (m_pressed) {
                m_handled = false;
                m_long_press = make_timeout_time_ms(400);
            } else {
                m_long_press = 0;
            }
        }
    }

    // Read and clear the pressed state
    inline bool pressed()
    {
        if (!m_handled && m_pressed) {
            m_handled = true;
            return true;
        }

        return false;
    }

    // Read and clear the long press state
    bool was_long_pressed()
    {
        if (!m_handled && m_long_press != 0 && get_absolute_time() > m_long_press) {
            m_long_press = 0;
            m_handled = true;
            return true;
        }

        return false;
    }

    // Read and clear the short press state
    bool was_short_pressed()
    {
        if (!m_handled && m_long_press == 0) {
            m_handled = true;
            return true;
        }

        return false;
    }

private:
    uint m_gpio;

    // Time to trigger a long press, or zero if not pressed
    absolute_time_t m_long_press = 0;
    bool m_pressed = false;
    bool m_handled = true;
};


int main()
{
    stdio_uart_init_full(uart0, 115200, PIN_UART_DEBUG_TX, -1);

    printf("\n\nDevice has reset\n");

	// GPIOs 0-11 as inputs with pull-up
	// (Looping as there's no mask function for direction)
	for (unsigned int pin = 1; pin <= 11; pin++) {
		gpio_init(pin);
		gpio_pull_up(pin);
		gpio_set_dir(pin, GPIO_IN);
	}

    // Send switch input is somewhere else
    gpio_init(PIN_SWITCH_SEND);
    gpio_pull_up(PIN_SWITCH_SEND);
    gpio_set_dir(PIN_SWITCH_SEND, GPIO_IN);

    // Configure switch LED PWM as off for loading
	{
        gpio_init(PIN_LED_PWM);
        gpio_put(PIN_LED_PWM, 1);
        gpio_set_dir(PIN_LED_PWM, GPIO_OUT);
        gpio_set_drive_strength(PIN_LED_PWM, GPIO_DRIVE_STRENGTH_12MA);
	}

	// Configure backlight PWM
    // This is just left on max brightness for now. The intention is to dim when in
	{
        gpio_init(PIN_DISP_BKLGHT);
        gpio_put(PIN_DISP_BKLGHT, 0);
        gpio_set_dir(PIN_DISP_BKLGHT, GPIO_OUT);

		gpio_set_function(PIN_DISP_BKLGHT, GPIO_FUNC_PWM);

	    const uint slice_num = pwm_gpio_to_slice_num(PIN_DISP_BKLGHT);
	    pwm_set_wrap(slice_num, 1024);
	    pwm_set_chan_level(slice_num, PWM_CHAN_A, 512);
	    pwm_set_enabled(slice_num, true);

        pwm_set_gpio_level(PIN_DISP_BKLGHT, 1024);
    }

	// Select pins to use for SPI0
	gpio_set_function(PIN_DISP_SPI_SCK, GPIO_FUNC_SPI);
	gpio_set_function(PIN_DISP_SPI_TX, GPIO_FUNC_SPI);

	// Enable SPI0 for screen
	spi_init(spi0, 32e6);

    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);


    // Set up and blank display
    st7789_init(&display_config, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // The particular screen I have has Y 0-20 outside of the display by default
    // Scroll so 0,0 in memory is actually rendered in the top-left corner
    st7789_vertical_scroll(300);

    // Start the application
    MainUI app;

    // Turn on data input LEDs (inverted as this drives a P-channel mosfet)
    {
        gpio_set_function(PIN_LED_PWM, GPIO_FUNC_PWM);

	    const uint slice_num = pwm_gpio_to_slice_num(PIN_LED_PWM);
	    pwm_set_wrap(slice_num, 1024);
	    pwm_set_chan_level(slice_num, PWM_CHAN_A, 512);
	    pwm_set_enabled(slice_num, true);

        // Very dim as the 100 Ohm resistors I'm using are a bit blinding
        pwm_set_gpio_level(PIN_LED_PWM, 1022);
    }

    uint8_t last_input = get_input_byte();
    app.set_codepoint(last_input);

    add_repeating_timer_ms(30, render_timer_callback, NULL, &render_timer);

    UserInput shift_switch(PIN_SWITCH_SHIFT);
    UserInput modeclear_switch(PIN_SWITCH_MODECLEAR);
    UserInput send_switch(PIN_SWITCH_SEND);

    while (true) {
        if (needs_render) {
            needs_render = false;

            // Handle inputs once per frame for now as it's easy
            const uint8_t input = get_input_byte();
            if (input != last_input) {
                last_input = input;
                app.set_low_byte(input);
            }

            shift_switch.update();
            modeclear_switch.update();
            send_switch.update();

            if (shift_switch.was_long_pressed()) {
                app.set_shift_lock(!app.get_shift_lock());
            } else if (shift_switch.was_short_pressed()) {
                app.shift();
            }
            
            if (modeclear_switch.was_long_pressed()) {
                app.reset();
            } else if (modeclear_switch.was_short_pressed()) {
                app.goto_next_mode();
            }

            if (send_switch.pressed()) {
                app.flush_buffer();
            }

            app.render();
        }
    }
}
