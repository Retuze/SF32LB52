/**
 * @file main.c
 * @brief Flash read speed benchmark — bootloader handles MPI2/MPU config.
 */
#include "hal.h"
#include <stdio.h>
#include <string.h>

#define SRAM_BUF_SIZE 10000
#define SEQ_REPS 100UL

#define MPI2_BASE     0x50042000UL
#define MPI2_CR       (*(volatile uint32_t*)(MPI2_BASE + 0x00))
#define MPI2_PSCLR    (*(volatile uint32_t*)(MPI2_BASE + 0x0C))
#define MPI2_SR       (*(volatile uint32_t*)(MPI2_BASE + 0x10))
#define MPI2_CCR1     (*(volatile uint32_t*)(MPI2_BASE + 0x28))
#define MPI2_HCMDR    (*(volatile uint32_t*)(MPI2_BASE + 0x40))
#define MPI2_HRCCR    (*(volatile uint32_t*)(MPI2_BASE + 0x48))
#define MPI2_HRABR    (*(volatile uint32_t*)(MPI2_BASE + 0x44))
#define MPI2_MISCR    (*(volatile uint32_t*)(MPI2_BASE + 0x58))
#define MPI2_TIMR     (*(volatile uint32_t*)(MPI2_BASE + 0x70))

static uint16_t sramBuf[SRAM_BUF_SIZE];
static uint16_t sramBuf2[SRAM_BUF_SIZE];
static const uint16_t flashData[SRAM_BUF_SIZE] __attribute__((aligned(4))) = {
    [0 ... SRAM_BUF_SIZE-1] = 0xABCD
};

/* =========================================================================
 * Unified copy benchmark — same algorithm, two locations (Flash vs RAM).
 * 32-bit word copy, identical code except for section placement.
 * ========================================================================= */

static SF32_RAMFUNC __attribute__((noinline))
void copy_words_ram(uint32_t* dst, const uint32_t* src, uint32_t words) {
    for (uint32_t i = 0; i < words; i++) dst[i] = src[i];
}

static __attribute__((noinline))
void copy_words_flash(uint32_t* dst, const uint32_t* src, uint32_t words) {
    for (uint32_t i = 0; i < words; i++) dst[i] = src[i];
}

static void bench_copy(const char* label, uint32_t bytes, uint32_t reps,
                       void (*copy_fn)(uint32_t*, const uint32_t*, uint32_t),
                       const uint32_t* src, uint32_t* dst) {
    uint32_t words = bytes / 4U;
    dst[0] = 0;  /* touch dst to ensure it's mapped */
    uint32_t t0 = DWT_CYCCNT;
    for (uint32_t r = 0; r < reps; r++) {
        copy_fn(dst, src, words);
        __asm volatile("" ::: "memory");
    }
    uint32_t us = (DWT_CYCCNT - t0) / 240UL;
    uint64_t total = (uint64_t)bytes * reps;
    uint32_t kBps = us ? (uint32_t)(total * 1000000UL / us / 1024UL) : 0UL;
    printf("  %-12s: %6lu us  %4lu KiB/s  (%lu KiB, dst0=0x%lx)\r\n",
           label, (unsigned long)us, (unsigned long)kBps,
           (unsigned long)(total / 1024UL), (unsigned long)dst[0]);
}

static void print_flash_diag(void) {
    uint32_t csr = HPSYS_RCC->CSR.R;
    uint32_t dll2 = HPSYS_RCC->DLL2CR.R;
    uint32_t hcmdr = MPI2_HCMDR;
    uint32_t hrccr = MPI2_HRCCR;

    printf("[flash] RCC CSR=0x%08lx DLL2CR=0x%08lx\r\n",
           (unsigned long)csr, (unsigned long)dll2);
    printf("[flash] MPI2 CR=0x%08lx PSCLR=%lu SR=0x%08lx CCR1=0x%08lx\r\n",
           (unsigned long)MPI2_CR, (unsigned long)MPI2_PSCLR,
           (unsigned long)MPI2_SR, (unsigned long)MPI2_CCR1);
    printf("[flash] AHB HCMDR=0x%08lx RCMD=0x%02lx HRCCR=0x%08lx HRABR=0x%08lx\r\n",
           (unsigned long)hcmdr, (unsigned long)(hcmdr & 0xFFUL),
           (unsigned long)hrccr, (unsigned long)MPI2_HRABR);
    printf("[flash] HRCCR imode=%lu admode=%lu adsize=%lu abmode=%lu absize=%lu dcyc=%lu dmode=%lu\r\n",
           (unsigned long)((hrccr >> 0) & 7UL),
           (unsigned long)((hrccr >> 3) & 7UL),
           (unsigned long)((hrccr >> 6) & 3UL),
           (unsigned long)((hrccr >> 8) & 7UL),
           (unsigned long)((hrccr >> 11) & 3UL),
           (unsigned long)((hrccr >> 13) & 31UL),
           (unsigned long)((hrccr >> 18) & 7UL));
    printf("[flash] MISCR=0x%08lx TIMR=0x%08lx\r\n",
           (unsigned long)MPI2_MISCR, (unsigned long)MPI2_TIMR);
}

/*
 * ARMv8-M (Cortex-M33) MPU registers — use RLAR not RASR.
 */
#define SCB_BASE  0xE000ED00UL
#define SCB_CCR   (*(volatile uint32_t*)(SCB_BASE + 0x14))
#define SCB_CCSIDR (*(volatile uint32_t*)(SCB_BASE + 0x80))
#define SCB_CSSELR (*(volatile uint32_t*)(SCB_BASE + 0x84))
#define SCB_ICIALLU (*(volatile uint32_t*)(SCB_BASE + 0x50))
#define SCB_DCISW   (*(volatile uint32_t*)(SCB_BASE + 0x60))

struct MPU_Regs {
    volatile uint32_t TYPE, CTRL, RNR, RBAR, RLAR;
};
#define MPU    ((struct MPU_Regs*)0xE000ED90UL)
#define MAIR0  (*(volatile uint32_t*)0xE000EDC0UL)
#define MAIR1  (*(volatile uint32_t*)0xE000EDC4UL)

/*
 * MAIR attribute byte values (ARMv8-M format).
 *   ATTR_NC  = Normal, Non-cacheable                      → 0x44
 *   ATTR_FLASH = Normal, Write-Through, Read-Allocate     → 0x22
 */
#define MAIR_ATTR_NC     (0x44U)
#define MAIR_ATTR_FLASH  (0x22U)

/*
 * ARMv8-M RBAR: BASE[31:5] | SH[4:3] | AP[2:1] | XN[0]
 *   BASE = base address (32-byte aligned)
 *   SH   = shareability (0=Non, 2=Outer, 3=Inner)
 *   AP   = access permission (2=RO any privilege, 0=RW privileged, 3=RW any)
 *   XN   = execute-never (0=executable)
 */
#define RBAR_ARMv8M(base, sh, ap, xn) \
    ((base) | (((sh) & 3U) << 3) | (((ap) & 3U) << 1) | ((xn) & 1U))

/*
 * ARMv8-M RLAR: LIMIT[31:5] | AttrIndx[3:1] | EN[0]
 *   LIMIT    = last byte address of region (32-byte aligned)
 *   AttrIndx = MAIR attribute index (0-7)
 *   EN       = region enable
 */
#define RLAR_ARMv8M(limit, idx) \
    (((limit) & 0xFFFFFFE0UL) | (((idx) & 7U) << 1) | 1U)

/* Enable I+D Cache + MPI2 prefetch.
 *
 * ARMv8-M (Cortex-M33) correct version:
 *   1. Disable caches + MPU, clear all regions
 *   2. MAIR Attr0=NC (unused), Attr1=Cacheable(WT+RA) for Flash
 *   3. MPU region 0: Flash XIP cacheable — everything else left to PRIVDEFENA
 *      default map (SRAM→Normal NC, peripheral→Device — the right defaults)
 *   4. Invalidate I+D caches, enable them in SCB CCR
 *   5. MPI2 prefetch window
 *
 * CRITICAL: runs from RAM (.ramfunc) because it modifies the Flash memory
 * attributes (MPU + caches) — executing from Flash while changing how Flash
 * is accessed causes an IACCVIOL fault.
 */
static SF32_RAMFUNC __attribute__((noinline))
void enable_flash_cache_prefetch(void)
{
    uint32_t i;

    /* 1. Tear down: disable caches, disable MPU, clear all 8 regions */
    __asm volatile("dsb"); __asm volatile("isb");
    SCB_CCR &= ~((1UL << 17) | (1UL << 16));
    __asm volatile("dsb"); __asm volatile("isb");

    MPU->CTRL = 0U;
    for (i = 0U; i < 8U; i++) { MPU->RNR = i; MPU->RLAR = 0U; }

    /* 2. MAIR: Attr0=NC (good default), Attr1=Cacheable WT+RA (for Flash) */
    MAIR0 = (MAIR_ATTR_FLASH << 8) | MAIR_ATTR_NC;
    MAIR1 = 0U;

    /* 3. Region 0: Flash XIP 0x12000000 32MB → cacheable.  SRAM &
     *    peripherals rely on PRIVDEFENA background map (correct Device
     *    semantics for bus peripherals, Normal NC for SRAM). */
    MPU->RNR  = 0U;
    MPU->RBAR = RBAR_ARMv8M(0x12000000U, 0, 2, 0);
    MPU->RLAR = RLAR_ARMv8M(0x13FFFFFFU, 1);

    /* 4. Enable MPU: EN | PRIVDEFENA */
    MPU->CTRL = (1U << 0) | (1U << 2);
    __asm volatile("dsb"); __asm volatile("isb");

    /* 5. Invalidate caches, then enable IC+DC */
    SCB_ICIALLU = 0U;
    __asm volatile("dsb"); __asm volatile("isb");

    /* D-cache invalidate by set/way (CSSELR already 0 from reset / cache_disable_all) */
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

int main(void)
{
    printf("\r\n[bench] Copy speed test\r\n");
    printf("[bench] HCLK=%lu Hz  PSCLR=%lu\r\n",
           (unsigned long)rcc_get_system_hz(),
           (unsigned long)(*(volatile uint32_t*)0x5004200CUL));
    print_flash_diag();

    const uint32_t copyBytes = (uint32_t)sizeof(flashData);  /* 20 KB */

    // --- Uncached pass (bootloader left cache/prefetch off) ---
    printf("=== Uncached ===\r\n");
    bench_copy("flash F->S", copyBytes, SEQ_REPS,
               copy_words_flash, (const uint32_t*)flashData, (uint32_t*)sramBuf2);
    bench_copy("flash S->S", copyBytes, SEQ_REPS,
               copy_words_flash, (const uint32_t*)sramBuf, (uint32_t*)sramBuf2);
    bench_copy("ram F->S", copyBytes, SEQ_REPS,
               copy_words_ram, (const uint32_t*)flashData, (uint32_t*)sramBuf2);
    bench_copy("ram S->S", copyBytes, SEQ_REPS,
               copy_words_ram, (const uint32_t*)sramBuf, (uint32_t*)sramBuf2);

    // --- Cached pass (MPU cacheable + MPI2 prefetch) ---
    enable_flash_cache_prefetch();
    printf("=== Cached + prefetch ===\r\n");
    bench_copy("flash F->S", copyBytes, SEQ_REPS,
               copy_words_flash, (const uint32_t*)flashData, (uint32_t*)sramBuf2);
    bench_copy("flash S->S", copyBytes, SEQ_REPS,
               copy_words_flash, (const uint32_t*)sramBuf, (uint32_t*)sramBuf2);
    bench_copy("ram F->S", copyBytes, SEQ_REPS,
               copy_words_ram, (const uint32_t*)flashData, (uint32_t*)sramBuf2);
    bench_copy("ram S->S", copyBytes, SEQ_REPS,
               copy_words_ram, (const uint32_t*)sramBuf, (uint32_t*)sramBuf2);

    printf("[bench] done\r\n");
    while (1) { delay(1000); }
}
