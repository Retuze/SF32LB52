/**
 * @file hal.h
 * @brief HAL umbrella header — include this from application code.
 *
 * Provides:
 *   - SoC register definitions (SF32LB52.h)
 *   - LL drivers: clock, GPIO, NVIC, pinmux, ATIM/PWM, UART
 *   - HAL wrapper: analogWrite (Arduino-compatible PWM)
 *   - Board pin map (board.h, from platform)
 */

#pragma once
#ifndef HAL_H
#define HAL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SoC */
#include "SF32LB52.h"

/* LL — low-level peripheral drivers */
#include "cache.h"
#include "clock.h"
#include "flash.h"
#include "ll_atim.h"
#include "ll_gpio.h"
#include "ll_nvic.h"
#include "ll_pinmux.h"
#include "ll_uart.h"

/* HAL — higher-level wrappers */
#include "hal_pwm.h"

/* Provided by each project's system.c */
void     SystemInit(void);
uint32_t millis(void);
void     delay(uint32_t ms);

#endif /* HAL_H */
