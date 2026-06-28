#pragma once
#ifndef CYCLE_COUNT_H
#define CYCLE_COUNT_H

#include <stdint.h>

/**
 * @file cycle_count.h
 * @brief Cycle-accurate performance counter (Cortex-M33 DWT).
 *
 *     uint32_t hz  = clk_get_hz();                // once
 *     uint32_t t0  = cycles();
 *     do_work();
 *     uint32_t us  = cycles_to_us(cycles() - t0, hz);
 *
 * DWT_CYCCNT is a free-running 32-bit counter at HCLK.  SystemInit()
 * already enables it — no extra init needed.
 */

/* Raw register (SF32LB52.h also defines DWT_CYCCNT) */
#ifndef DWT_CYCCNT
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004UL)
#endif

static inline uint32_t cycles(void)
{
    return DWT_CYCCNT;
}

static inline uint32_t cycles_to_us(uint32_t cyc, uint32_t hclk_hz)
{
    return (uint32_t)(((uint64_t)cyc * 1000000ULL) / (uint64_t)hclk_hz);
}

#endif /* CYCLE_COUNT_H */
