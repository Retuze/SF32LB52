/**
 * @file board.h
 * @brief Hardware variant: Watch V1.0 (SF32LB52-DevKit-Nano based)
 *
 * Pin mappings, onboard peripherals, and board-level capabilities.
 * Each PCB revision gets its own platforms/watch_vX_Y/ directory.
 */

#pragma once
#ifndef _BOARD_H_
#define _BOARD_H_

#include <stdint.h>

/* ==========================================================================
 * Display (QSPI bit-bang)
 * ========================================================================== */
#define LCD_BL   1
#define LCD_CS   3
#define LCD_RST  0
#define LCD_TE   2
#define LCD_CLK  4
#define LCD_D0   5
#define LCD_D1   6
#define LCD_D2   7
#define LCD_D3   8

#define LCD_WIDTH  390
#define LCD_HEIGHT 450

typedef struct lcd_device lcd_device_t;
extern lcd_device_t lcd0;

/* ==========================================================================
 * Touch panel (I2C)
 * ========================================================================== */
#define CTP_INT 9
#define CTP_RST 10
#define CTP_SDA 11
#define CTP_SCL 20

/* ==========================================================================
 * LEDs
 * ========================================================================== */
#define LED_RED   31
#define LED_GREEN 32

/* ==========================================================================
 * Heart-rate sensor (I2C) — populated on V1.0+
 * ========================================================================== */
#define HRM_INT  33
#define HRM_SDA  34
#define HRM_SCL  35

/* ==========================================================================
 * Display backlight PWM channel
 * ========================================================================== */
#define LCD_BL_PWM_CH  ATIM_CH1

#endif /* _BOARD_H_ */
