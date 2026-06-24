#include "hal_gpio.h"
#include "ll_gpio.h"
#include "ll_pinmux.h"

static uint32_t gpio_input_flags(uint8_t mode)
{
    switch (mode) {
    case INPUT_PULLUP:
        return SF32_PINMUX_PULL_UP | SF32_PINMUX_INPUT_ENABLE;
    case INPUT_PULLDOWN:
        return SF32_PINMUX_PULL_DOWN | SF32_PINMUX_INPUT_ENABLE;
    case INPUT:
    default:
        return SF32_PINMUX_PULL_NONE | SF32_PINMUX_INPUT_ENABLE;
    }
}

void pinMode(uint32_t pin, uint8_t mode)
{
    sf32lb52_gpio_enable_clock();

    if (mode == OUTPUT) {
        sf32lb52_pinmux_config_pad(pin, 0U,
                                   SF32_PINMUX_PULL_NONE | SF32_PINMUX_DRIVE_3);
        sf32lb52_gpio_enable_output(pin);
        return;
    }

    sf32lb52_pinmux_config_pad(pin, 0U, gpio_input_flags(mode));
    sf32lb52_gpio_disable_output(pin);
}

void digitalWrite(uint32_t pin, uint8_t value)
{
    sf32lb52_gpio_write(pin, value);
}

uint8_t digitalRead(uint32_t pin)
{
    return sf32lb52_gpio_read(pin) ? HIGH : LOW;
}

void digitalToggle(uint32_t pin)
{
    sf32lb52_gpio_toggle(pin);
}
