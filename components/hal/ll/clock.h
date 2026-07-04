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

/* ── DWT cycle counter (Cortex-M DWT_CYCCNT @ HCLK) ────────────────────── */

/**
 * Read DWT_CYCCNT (free-running 32-bit cycle counter at HCLK).
 * SystemInit() already enables it — no extra init needed.
 */
uint32_t dwt_cycles(void);

/**
 * Convert cycle count to microseconds.
 * @param cyc      Cycle count (e.g., dwt_cycles() - t0)
 * @param hclk_hz  HCLK frequency (from clk_get_hz())
 */
uint32_t dwt_cycles_to_us(uint32_t cyc, uint32_t hclk_hz);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_H */
