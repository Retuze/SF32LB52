#pragma once
#ifndef LL_GPIO_H
#define LL_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sf32lb52_gpio_enable_clock(void);
void sf32lb52_gpio_enable_output(uint32_t pin);
void sf32lb52_gpio_disable_output(uint32_t pin);
void sf32lb52_gpio_write(uint32_t pin, uint8_t value);
uint8_t sf32lb52_gpio_read(uint32_t pin);
void sf32lb52_gpio_toggle(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif
