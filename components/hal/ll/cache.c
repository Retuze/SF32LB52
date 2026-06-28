/**
 * @file cache.c
 * @brief Enable I-Cache, D-Cache, MPU, and MPI2 Flash prefetch.
 *
 * CRITICAL: runs from RAM (.ramfunc) because it modifies Flash's MPU
 * attributes while the CPU may be fetching from Flash.
 */

#include "cache.h"
#include "SF32LB52.h"
#include <stdint.h>

#define SCB_CCR     (*(volatile uint32_t*)0xE000ED14UL)
#define SCB_CCSIDR  (*(volatile uint32_t*)0xE000ED80UL)
#define SCB_CSSELR  (*(volatile uint32_t*)0xE000ED84UL)
#define SCB_ICIALLU (*(volatile uint32_t*)0xE000ED50UL)
#define SCB_DCISW   (*(volatile uint32_t*)0xE000ED60UL)

struct mpu_regs { volatile uint32_t TYPE, CTRL, RNR, RBAR, RLAR; };
#define MPU   ((struct mpu_regs*)0xE000ED90UL)
#define MAIR0 (*(volatile uint32_t*)0xE000EDC0UL)
#define MAIR1 (*(volatile uint32_t*)0xE000EDC4UL)

#define MPI2_BASE       0x50042000UL
#define MAIR_ATTR_NC    0x44U
#define MAIR_ATTR_FLASH 0x22U

RAMFUNC __attribute__((noinline))
void cache_enable(void)
{
    /* 1. Disable caches + MPU */
    __asm volatile("dsb"); __asm volatile("isb");
    SCB_CCR &= ~((1UL << 17) | (1UL << 16));
    __asm volatile("dsb"); __asm volatile("isb");

    MPU->CTRL = 0U;
    for (uint32_t i = 0U; i < 8U; i++) { MPU->RNR = i; MPU->RLAR = 0U; }

    /* 2. MAIR: Attr0=NC, Attr1=WT+RA (Flash) */
    MAIR0 = (MAIR_ATTR_FLASH << 8) | MAIR_ATTR_NC;
    MAIR1 = 0U;

    /* 3. Region 0: Flash XIP (0x12000000–0x13FFFFFF) cacheable */
    MPU->RNR  = 0U;
    MPU->RBAR = (0x12000000U) | (0U << 3) | (2U << 1) | 0U;  /* sh=0, ap=2, xn=0 */
    MPU->RLAR = ((0x13FFFFFFU) & 0xFFFFFFE0UL) | (1U << 1) | 1U;  /* attr1, enable */

    /* 4. Enable MPU: EN | PRIVDEFENA */
    MPU->CTRL = (1U << 0) | (1U << 2);
    __asm volatile("dsb"); __asm volatile("isb");

    /* 5. Invalidate + enable IC+DC */
    SCB_ICIALLU = 0U;
    __asm volatile("dsb"); __asm volatile("isb");

    SCB_CSSELR = 0U;
    __asm volatile("dsb");
    {
        uint32_t c  = SCB_CCSIDR;
        uint32_t nw = ((c >> 3) & 0x3FFU) + 1U;
        uint32_t ns = ((c >> 13) & 0x7FFFU) + 1U;
        for (uint32_t w = 0U; w < nw; w++)
            for (uint32_t s = 0U; s < ns; s++)
                SCB_DCISW = (w << 30) | (s << 5);
    }
    __asm volatile("dsb"); __asm volatile("isb");

    SCB_CCR |= (1UL << 17) | (1UL << 16);
    __asm volatile("dsb"); __asm volatile("isb");

    /* 6. MPI2 prefetch window (0x12000000–0x14000000) */
    (*(volatile uint32_t*)(MPI2_BASE + 0x008C)) = 0x12000000UL >> 10;
    (*(volatile uint32_t*)(MPI2_BASE + 0x0090)) = 0x14000000UL >> 10;
    (*(volatile uint32_t*)(MPI2_BASE + 0x0000)) |= (1UL << 22);
    __asm volatile("dsb"); __asm volatile("isb");
}
