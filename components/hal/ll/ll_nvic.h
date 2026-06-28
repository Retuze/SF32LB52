#pragma once
#ifndef NVIC_H
#define NVIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nvic.h
 * @brief Cortex-M33 NVIC — enable IRQ / set priority.
 *
 *     nvic_set_priority(GPIO1_IRQn, (2U << 6) | 1U);
 *     nvic_enable_irq(GPIO1_IRQn);
 */

static inline void nvic_enable_irq(uint32_t irqn)
{
    volatile uint32_t *iser = (volatile uint32_t *)(0xE000E100UL + ((irqn >> 5U) << 2U));
    *iser = 1UL << (irqn & 0x1FUL);
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
}

static inline void nvic_set_priority(uint32_t irqn, uint32_t prio)
{
    volatile uint8_t *ipr = (volatile uint8_t *)(0xE000E400UL + irqn);
    *ipr = (uint8_t)prio;
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
}

#ifdef __cplusplus
}
#endif

#endif /* NVIC_H */
