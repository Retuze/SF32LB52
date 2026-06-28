#pragma once
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file clock.h
 * @brief System clock control — query / set HCLK frequency.
 *
 *     clk_set_hz(HCLK_240MHZ);
 *     uint32_t hz = clk_get_hz();
 */

#define HCLK_48MHZ    48000000UL
#define HCLK_240MHZ  240000000UL
#define HCLK_MAX     240000000UL

void     clk_set_hz(uint32_t hz);
uint32_t clk_get_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_H */
