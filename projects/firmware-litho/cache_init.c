/**
 * @file cache_init.c
 * @brief Enable I+D Cache + MPI2 prefetch — runs from RAM (.ramfunc).
 *
 * CRITICAL: must execute from RAM because it modifies Flash's MPU attributes
 * while the CPU is fetching instructions from Flash.  Running this code on
 * Flash causes an IACCVIOL (instruction access violation).
 */
#include "hal.h"
#include <stdint.h>

#define SCB_BASE    0xE000ED00UL
#define SCB_CCR     (*(volatile uint32_t*)(SCB_BASE + 0x14))
#define SCB_CCSIDR  (*(volatile uint32_t*)(SCB_BASE + 0x80))
#define SCB_CSSELR  (*(volatile uint32_t*)(SCB_BASE + 0x84))
#define SCB_ICIALLU (*(volatile uint32_t*)(SCB_BASE + 0x50))
#define SCB_DCISW   (*(volatile uint32_t*)(SCB_BASE + 0x60))

struct MPU_Regs { volatile uint32_t TYPE, CTRL, RNR, RBAR, RLAR; };
#define MPU    ((struct MPU_Regs*)0xE000ED90UL)
#define MAIR0  (*(volatile uint32_t*)0xE000EDC0UL)
#define MAIR1  (*(volatile uint32_t*)0xE000EDC4UL)

#define MPI2_BASE 0x50042000UL

#define MAIR_ATTR_NC    (0x44U)   /* Normal, Non-cacheable */
#define MAIR_ATTR_FLASH (0x22U)   /* Normal, Write-Through, Read-Allocate */

#define RBAR_ARMv8M(base, sh, ap, xn) \
    ((base) | (((sh) & 3U) << 3) | (((ap) & 3U) << 1) | ((xn) & 1U))

#define RLAR_ARMv8M(limit, idx) \
    (((limit) & 0xFFFFFFE0UL) | (((idx) & 7U) << 1) | 1U)

SF32_RAMFUNC __attribute__((noinline))
void enable_flash_cache_prefetch(void)
{
    uint32_t i;

    /* 1. Disable caches + MPU, clear all regions */
    __asm volatile("dsb"); __asm volatile("isb");
    SCB_CCR &= ~((1UL << 17) | (1UL << 16));
    __asm volatile("dsb"); __asm volatile("isb");

    MPU->CTRL = 0U;
    for (i = 0U; i < 8U; i++) { MPU->RNR = i; MPU->RLAR = 0U; }

    /* 2. MAIR: Attr0=NC, Attr1=Cacheable WT+RA (for Flash) */
    MAIR0 = (MAIR_ATTR_FLASH << 8) | MAIR_ATTR_NC;
    MAIR1 = 0U;

    /* 3. Region 0: Flash XIP cacheable */
    MPU->RNR  = 0U;
    MPU->RBAR = RBAR_ARMv8M(0x12000000U, 0, 2, 0);
    MPU->RLAR = RLAR_ARMv8M(0x13FFFFFFU, 1);

    /* 4. Enable MPU: EN | PRIVDEFENA */
    MPU->CTRL = (1U << 0) | (1U << 2);
    __asm volatile("dsb"); __asm volatile("isb");

    /* 5. Invalidate caches, enable IC+DC */
    SCB_ICIALLU = 0U;
    __asm volatile("dsb"); __asm volatile("isb");

    SCB_CSSELR = 0U;
    __asm volatile("dsb");
    {
        uint32_t c = SCB_CCSIDR;
        uint32_t nw = ((c >> 3) & 0x3FFU) + 1U;
        uint32_t ns = ((c >> 13) & 0x7FFFU) + 1U;
        for (uint32_t w = 0U; w < nw; w++)
            for (uint32_t s = 0U; s < ns; s++)
                SCB_DCISW = (w << 30) | (s << 5);
    }
    __asm volatile("dsb"); __asm volatile("isb");

    SCB_CCR |= (1UL << 17) | (1UL << 16);
    __asm volatile("dsb"); __asm volatile("isb");

    /* 6. MPI2 prefetch window */
    (*(volatile uint32_t*)(MPI2_BASE + 0x008C)) = 0x12000000UL >> 10;
    (*(volatile uint32_t*)(MPI2_BASE + 0x0090)) = 0x14000000UL >> 10;
    (*(volatile uint32_t*)(MPI2_BASE + 0x0000)) |= (1UL << 22);
    __asm volatile("dsb"); __asm volatile("isb");
}
