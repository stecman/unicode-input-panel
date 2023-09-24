#include "st7789.h"
#include "ui/main_ui.hh"
#include "usb.h"

// Pico SDK
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/stdio_usb.h"
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
    UserInput(uint32_t gpio) : m_gpio(gpio) {}

    void update()
    {
        const bool pressed = gpio_get(m_gpio) == 0;

        if (pressed != m_pressed) {
            m_pressed = pressed;

            if (m_pressed) {
                m_handled = false;
                m_long_press = make_timeout_time_ms(400);
            } else {
                m_long_press = nil_time;
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
        if (!m_handled && m_long_press != nil_time && get_absolute_time() > m_long_press) {
            m_long_press = 0;
            m_handled = true;
            return true;
        }

        return false;
    }

    // Read and clear the short press state
    bool was_short_pressed()
    {
        if (!m_handled && m_long_press == nil_time) {
            m_handled = true;
            return true;
        }

        return false;
    }

private:
    uint32_t m_gpio;

    // Time to trigger a long press, or zero if not pressed
    absolute_time_t m_long_press = nil_time;
    bool m_pressed = false;
    bool m_handled = true;
};


// Use keycode conversion table provided by the Pico SDK
static constexpr size_t kAsciiToKeycodeLength = 128;
static constexpr uint8_t kAsciiToKeycode[kAsciiToKeycodeLength][2] = { HID_ASCII_TO_KEYCODE };

class CodepointSender
{
public:
    void send(uint32_t codepoint)
    {
        constexpr uint8_t indexShift = 0;
        constexpr uint8_t indexKeycode = 1;

        // TODO: handle send send buffer full

        // Send a single key if the codepoint is valid ASCII
        // Note this will be incompatible with non-US keyboard layouts
        if (codepoint < kAsciiToKeycodeLength) {
            uint8_t keycode = kAsciiToKeycode[codepoint][indexKeycode];
            if (keycode != 0) {
                uint8_t modifiers = kAsciiToKeycode[codepoint][indexShift] ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;
                m_key_queue[m_queue_size++] = KeyPress(keycode, modifiers);
                start_sending();
                return;
            }
        }

        // Convert to a sequence of key presses for a specific operating system
        // TODO: Currently this only supports Linux Ctrl+Shift+U sequence entry
        char _buf[12];
        char* str = (char*) &_buf;
        sprintf(str, "%X", codepoint);

        // Start unicode hex entry (Linux desktop)
        m_key_queue[m_queue_size++] = KeyPress(HID_KEY_U, KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT);

        while (*str != '\0') {
            uint8_t keycode = kAsciiToKeycode[*str][indexKeycode];
            m_key_queue[m_queue_size++] = KeyPress(keycode);
            str++;
        }

        // Finish sequence with enter
        m_key_queue[m_queue_size++] = KeyPress(HID_KEY_ENTER);

        start_sending();
    }

    // Returns true when sending has completed
    bool update()
    {
        if (m_waiting && !usb_last_report_sent()) {
            // Waiting between reports
            return false;
        }

        m_waiting = false;

        if (m_needs_release) {
            // Send a report with the previous key press released
            send_release();
            return false;
        }

        if (m_queue_size == 0) {
            // Stop sending
            return true;
        }

        // Send next key press, or reset
        if (m_index < m_queue_size) {
            auto& press = m_key_queue[m_index++];
            send_key(press.keycode, press.modifiers);

        } else {
            // No more keys to send
            m_queue_size = 0;
            m_index = 0;

            // Stop sending
            return true;
        }

        return false;
    }

private:
    void start_sending()
    {
        // Start sending keys if it's not running yet
        if (m_send_timer.alarm_id == 0) {
            add_repeating_timer_ms(1, key_send_callback, this, &m_send_timer);
            update();
        }
    }

    static bool key_send_callback(struct repeating_timer *t)
    {
        auto* sender = reinterpret_cast<CodepointSender*>(t->user_data);
        return !sender->update(); // Update and cancel if no more to send
    }

    void send_key(uint8_t keycode, uint8_t modifiers = 0)
    {
        m_keymap[0] = keycode;
        usb_set_key_report(m_keymap, modifiers);

        m_needs_release = true;
        m_waiting = true;
    }

    void send_release()
    {
        m_keymap[0] = 0;
        usb_set_key_report(m_keymap, 0);
        m_needs_release = false;
        m_waiting = true;
    }

    struct KeyPress {
        KeyPress() : keycode(0), modifiers(0) {}
        KeyPress(uint8_t keycode) : keycode(keycode), modifiers(0) {}
        KeyPress(uint8_t keycode, uint8_t modifiers) : keycode(keycode), modifiers(modifiers) {}

        uint8_t keycode;
        uint8_t modifiers;
    };

    struct repeating_timer m_send_timer;

    uint32_t m_index = 0;
    uint32_t m_queue_size = 0;
    KeyPress m_key_queue[16];
    bool m_waiting = false;
    bool m_needs_release = false;

    uint8_t m_keymap[6]= {0, 0, 0, 0, 0, 0};
};


bool background_usb_poll(struct repeating_timer *t)
{
    usb_poll();
    return true;
}

int main()
{
    static MainUI app;
    static CodepointSender sender;

    usb_init();
    stdio_usb_init();

    // Run USB polling from an interrupt as the main loop slows down when rendering
    static struct repeating_timer usb_timer;
    add_repeating_timer_ms(5, background_usb_poll, NULL, &usb_timer);

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

	    const uint32_t slice_num = pwm_gpio_to_slice_num(PIN_DISP_BKLGHT);
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

    const uint32_t LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);


    // Set up and blank display
    st7789_init(&display_config, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // The particular screen I have has Y 0-20 outside of the display by default
    // Scroll so 0,0 in memory is actually rendered in the top-left corner
    st7789_vertical_scroll(300);

    // Start the application (blocking until loaded)
    app.load("fonts");

    // Turn on data input LEDs (inverted as this drives a P-channel mosfet)
    {
        gpio_set_function(PIN_LED_PWM, GPIO_FUNC_PWM);

	    const uint32_t slice_num = pwm_gpio_to_slice_num(PIN_LED_PWM);
	    pwm_set_wrap(slice_num, 1024);
	    pwm_set_chan_level(slice_num, PWM_CHAN_A, 512);
	    pwm_set_enabled(slice_num, true);

        // Very dim as the 100 Ohm resistors I'm using are a bit blinding
        pwm_set_gpio_level(PIN_LED_PWM, 1022);
    }

    uint8_t last_input = get_input_byte();
    app.set_low_byte(last_input);

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
                app.toggle_shift_lock();
            } else if (shift_switch.was_short_pressed()) {
                app.shift();
            }

            if (modeclear_switch.was_long_pressed()) {
                app.reset();
            } else if (modeclear_switch.was_short_pressed()) {
                app.goto_next_mode(input);
            }

            if (send_switch.pressed()) {
                for (uint32_t codepoint : app.get_codepoints()) {
                    sender.send(codepoint);
                }
            }

            app.tick();
        }
    }
}
