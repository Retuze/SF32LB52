#pragma once
#ifndef LL_GPIO_H
#define LL_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ll_gpio.h
 * @brief GPIO driver — Arduino-compatible API.
 *
 *     pinMode(pin, INPUT / OUTPUT / INPUT_PULLUP / INPUT_PULLDOWN / OUTPUT_OPENDRAIN)
 *     digitalWrite(pin, LOW / HIGH)
 *     digitalRead(pin)
 *     digitalToggle(pin)
 *
 *     attachInterrupt(pin, callback, CHANGE / FALLING / RISING, arg)
 *     detachInterrupt(pin)
 *
 * The central GPIO1_IRQHandler (ll_gpio.c) dispatches pending pins to
 * registered callbacks. Drivers never write their own ISR.
 */

/* ── Pin modes ─────────────────────────────────────────────────────────── */
#define INPUT             0
#define OUTPUT            1
#define INPUT_PULLUP      2
#define INPUT_PULLDOWN    3
#define OUTPUT_OPENDRAIN  4

#define LOW  0
#define HIGH 1

/* ── Basic I/O ──────────────────────────────────────────────────────────── */
void     pinMode(uint32_t pin, uint8_t mode);
void     digitalWrite(uint32_t pin, uint8_t value);
uint8_t  digitalRead(uint32_t pin);
void     digitalToggle(uint32_t pin);

/* ── Interrupt ──────────────────────────────────────────────────────────── */
#define CHANGE   1U    /* both edges  */
#define FALLING  2U    /* falling edge */
#define RISING   3U    /* rising edge */

typedef void (*gpio_irq_cb_t)(uint32_t pin, void *arg);

void     attachInterrupt(uint32_t pin, gpio_irq_cb_t cb, uint32_t mode, void *arg);
void     detachInterrupt(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif /* LL_GPIO_H */
