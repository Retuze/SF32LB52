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
 * Display
 * ========================================================================== */
enum {
    LCD_BL   = 1,
    LCD_CS   = 3,
    LCD_RST  = 0,
    LCD_TE   = 2,
    LCD_CLK  = 4,
    LCD_D0   = 5,
    LCD_D1   = 6,
    LCD_D2   = 7,
    LCD_D3   = 8,

    LCD_WIDTH  = 390,
    LCD_HEIGHT = 450,
};

/* ==========================================================================
 * Touch panel (I2C)
 * ========================================================================== */
enum {
    CTP_INT = 9,
    CTP_RST = 10,
    CTP_SDA = 11,
    CTP_SCL = 20,
};

/* ==========================================================================
 * LEDs
 * ========================================================================== */
enum {
    LED_RED   = 31,
    LED_GREEN = 32,
};

/* ==========================================================================
 * Heart-rate sensor (I2C) — populated on V1.0+
 * ========================================================================== */
enum {
    HRM_INT = 33,
    HRM_SDA = 34,
    HRM_SCL = 35,
};

/* ==========================================================================
 * Display backlight PWM channel
 * ========================================================================== */
enum {
    LCD_BL_PWM_CH = 1,  // ATIM channel 1
};

/* ==========================================================================
 * Board initialization functions
 * ========================================================================== */

/**
 * @brief Initialize LCD (bus + IC + geometry + pins).
 *
 * Configures and initializes the LCD subsystem for this board variant.
 * Encapsulates lcd_set_bus/ic/resolution/ctrl_pins/init sequence.
 */
void board_lcd_init(void);

/**
 * @brief Initialize touch panel (bus + IC + geometry + pins).
 *
 * Configures and initializes the TP subsystem for this board variant.
 * Encapsulates tp_set_bus/ic/resolution/ctrl_pins/init sequence.
 */
void board_tp_init(void);

#endif /* _BOARD_H_ */
