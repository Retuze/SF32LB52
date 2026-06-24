/**
 * @file startup.c
 * @brief Reset handler, vector table, data/bss init, TLS setup
 *
 * Shared across all bare-metal projects.  Link with picolibc + platform link.ld.
 */

#include <stddef.h>
#include <stdint.h>

#include <picotls.h>
#include "hal.h"

extern size_t __StackTop;
extern size_t __StackLimit;
extern size_t __data_load__;
extern size_t __data_start__;
extern size_t __data_end__;
extern size_t __ramfunc_load__ __attribute__((weak));
extern size_t __ramfunc_start__ __attribute__((weak));
extern size_t __ramfunc_end__ __attribute__((weak));
extern size_t __bss_start__;
extern size_t __bss_end__;
extern size_t __tbss_start;
extern size_t __tbss_end;
extern char   __tls_base[];

extern int main(void);

static void Default_Handler(void)
{
    while (1) {
    }
}

void Reset_Handler(void)
{
    size_t *src = &__data_load__;
    size_t *dst = &__data_start__;

    __asm volatile("msr msplim, %0" : : "r"(&__StackLimit) : "memory");

    /* Copy .data from ROM to RAM */
    while (dst < &__data_end__) {
        *dst++ = *src++;
    }

    /* Copy .ramfunc if present */
    if (&__ramfunc_start__ != 0 && &__ramfunc_end__ != 0 && &__ramfunc_load__ != 0) {
        src = &__ramfunc_load__;
        dst = &__ramfunc_start__;
        while (dst < &__ramfunc_end__) {
            *dst++ = *src++;
        }
    }

    /* Zero .bss */
    dst = &__bss_start__;
    while (dst < &__bss_end__) {
        *dst++ = 0;
    }

    /* Zero .tbss */
    dst = (size_t *)&__tbss_start;
    while (dst < (size_t *)&__tbss_end) {
        *dst++ = 0;
    }

    _init_tls(__tls_base);
    _set_tls(__tls_base);

    /* Call C++ static constructors (init_array). */
    extern void (*__init_array_start[])(void);
    extern void (*__init_array_end[])(void);
    for (void (**p)(void) = __init_array_start; p < __init_array_end; p++) {
        (*p)();
    }

    SystemInit();
    __asm volatile("cpsie i" ::: "memory");
    main();

    while (1) {
    }
}

/* ---- Weak default handlers (overridable by application) ---- */
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
extern void SysTick_Handler(void);

/* ---- Vector table (Cortex-M33, 16 system exceptions) ---- */
__attribute__((section(".isr_vector"), used))
const uintptr_t g_pfnVectors[] = {
    (uintptr_t)&__StackTop,         /*  0: Initial MSP */
    (uintptr_t)Reset_Handler,       /*  1: Reset */
    (uintptr_t)NMI_Handler,         /*  2: NMI */
    (uintptr_t)HardFault_Handler,   /*  3: HardFault */
    (uintptr_t)MemManage_Handler,   /*  4: MemManage */
    (uintptr_t)BusFault_Handler,    /*  5: BusFault */
    (uintptr_t)UsageFault_Handler,  /*  6: UsageFault */
    0U,                             /*  7: (reserved) */
    0U,                             /*  8: (reserved) */
    0U,                             /*  9: (reserved) */
    0U,                             /* 10: (reserved) */
    (uintptr_t)SVC_Handler,         /* 11: SVCall */
    (uintptr_t)DebugMon_Handler,    /* 12: DebugMon */
    0U,                             /* 13: (reserved) */
    (uintptr_t)PendSV_Handler,      /* 14: PendSV */
    (uintptr_t)SysTick_Handler,     /* 15: SysTick */
};
