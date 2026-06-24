/**
 * @file hal.h
 * @brief HAL umbrella header — includes all chip-specific headers.
 *
 * Include this single header in application code to get:
 *   - SoC register definitions (SF32LB52.h)
 *   - LL drivers (ll_*.h)
 *   - HAL peripheral drivers (hal_*.h)
 *   - Board pin mappings (board.h, from platform)
 *
 * Usage:
 *   #include "hal.h"
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

/* HAL */
#include "hal_afio.h"
#include "hal_gpio.h"
#include "hal_pwm.h"
#include "hal_uart.h"

/* LL */
#include "ll_dwt.h"
#include "ll_nvic.h"
#include "ll_rcc.h"
#include "ll_rtt.h"
#include "ll_uart.h"

/* Provided by each project's system.c */
void SystemInit(void);
uint32_t millis(void);
void delay(uint32_t ms);

#endif
