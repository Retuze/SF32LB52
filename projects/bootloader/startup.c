/**
 * @file startup.c — Bootloader reset handler & vector table.
 *
 * Lightweight version: no .tdata/.ramfunc handling needed for bootloader.
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
extern size_t __bss_start__;
extern size_t __bss_end__;
extern size_t __tbss_start;
extern size_t __tbss_end;
extern char   __tls_base[];

extern int main(void);

static void Default_Handler(void)
{
    while (1) {}
}

void Reset_Handler(void)
{
    size_t *src = &__data_load__;
    size_t *dst = &__data_start__;

    __asm volatile("msr msplim, %0" : : "r"(&__StackLimit) : "memory");

    while (dst < &__data_end__) {
        *dst++ = *src++;
    }

    dst = &__bss_start__;
    while (dst < &__bss_end__) {
        *dst++ = 0;
    }

    dst = (size_t *)&__tbss_start;
    while (dst < (size_t *)&__tbss_end) {
        *dst++ = 0;
    }

    _init_tls(__tls_base);
    _set_tls(__tls_base);

    SystemInit();
    main();

    while (1) {}
}

void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
extern void SysTick_Handler(void);

__attribute__((section(".isr_vector"), used))
const uintptr_t g_pfnVectors[] = {
    (uintptr_t)&__StackTop,         /*  0 */
    (uintptr_t)Reset_Handler,       /*  1 */
    (uintptr_t)NMI_Handler,         /*  2 */
    (uintptr_t)HardFault_Handler,   /*  3 */
    (uintptr_t)MemManage_Handler,   /*  4 */
    (uintptr_t)BusFault_Handler,    /*  5 */
    (uintptr_t)UsageFault_Handler,  /*  6 */
    0U, 0U, 0U, 0U,
    (uintptr_t)SVC_Handler,         /* 11 */
    (uintptr_t)DebugMon_Handler,    /* 12 */
    0U,
    (uintptr_t)PendSV_Handler,      /* 14 */
    (uintptr_t)SysTick_Handler,     /* 15 */
};
