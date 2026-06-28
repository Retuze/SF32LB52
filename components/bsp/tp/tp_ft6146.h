#pragma once
#ifndef TP_FT6146_H
#define TP_FT6146_H

#include <stdint.h>
#include <stdbool.h>
#include "bb_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file tp_ft6146.h
 * @brief FT6146-M00 touch panel driver via bit-bang I2C.
 *
 * Touch event values align with litho::TouchAction: 0=DOWN, 1=MOVE, 2=UP.
 */

#define TP_FT6146_MAX_POINTS  2

/**
 * @brief ISR → pollEvent flag. Set by GPIO1_IRQHandler, cleared by SF32Input::pollEvent().
 */
extern volatile int g_tp_irq_fired;

typedef struct {
    bb_i2c_t    i2c;         /* I2C bus config (SDA/SCL + optional delay) */
    uint32_t    pin_int;     /* CTP_INT — active-low interrupt from touch IC */
    uint32_t    pin_rst;     /* CTP_RST — reset output */
    uint16_t    max_x;       /* panel physical width (for coordinate mirroring) */
    uint16_t    max_y;       /* panel physical height */
} tp_ft6146_t;

/**
 * @brief Initialize FT6146: power-on sequence, chip ID check, IRQ config.
 * @param tp  Pointer to device config (pins, i2c bus, panel size).
 */
void tp_ft6146_init(tp_ft6146_t *tp);

/**
 * @brief Read single touch point from FT6146 (call only when CTP_INT asserted).
 * @param tp       Device config.
 * @param out_x    [out] Touch X coordinate.
 * @param out_y    [out] Touch Y coordinate.
 * @param out_event [out] 0=DOWN, 1=MOVE, 2=UP.
 * @return 0 on success, negative on I2C error.
 */
int tp_ft6146_read(tp_ft6146_t *tp, int *out_x, int *out_y, int *out_event);

/**
 * @brief Query whether CTP_INT is asserted (touch event pending).
 * @return true if CTP_INT is LOW (touch present).
 */
bool tp_ft6146_touched(tp_ft6146_t *tp);

#ifdef __cplusplus
}
#endif

#endif /* TP_FT6146_H */
