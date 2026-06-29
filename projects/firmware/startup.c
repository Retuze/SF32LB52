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

#define USART_ISR_TXE  (1UL << 7)
#define USART_ISR_TC   (1UL << 6)

static void early_uart_putc(char c)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0U) {}
    USART1->TDR = (uint8_t)c;
    while ((USART1->ISR & USART_ISR_TC) == 0U) {}
}

static void early_uart_puts(const char *s)
{
    while (*s) { early_uart_putc(*s++); }
}

static void early_uart_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    early_uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        early_uart_putc(hex[(v >> i) & 0xFU]);
    }
}

/* Fault status registers (Cortex-M33) */
#define SCB_CFSR  (*(volatile uint32_t*)0xE000ED28U)
#define SCB_HFSR  (*(volatile uint32_t*)0xE000ED2CU)
#define SCB_MMFAR (*(volatile uint32_t*)0xE000ED34U)
#define SCB_BFAR  (*(volatile uint32_t*)0xE000ED38U)

static void Default_Handler(void)
{
    uint32_t cfsr = SCB_CFSR;
    uint32_t hfsr = SCB_HFSR;
    uint32_t mmfar = SCB_MMFAR;
    uint32_t bfar  = SCB_BFAR;

    early_uart_puts("\r\n! FAULT ");
    early_uart_puts("CFSR=");  early_uart_hex32(cfsr);
    early_uart_puts(" HFSR="); early_uart_hex32(hfsr);
    early_uart_puts(" MMFAR="); early_uart_hex32(mmfar);
    early_uart_puts(" BFAR=");  early_uart_hex32(bfar);
    early_uart_puts("\r\n");

    /* Decode common fault types */
    if (cfsr & (1U << 25)) { early_uart_puts("  DIVBYZERO\r\n"); }
    if (cfsr & (1U << 24)) { early_uart_puts("  UNALIGNED\r\n"); }
    if (cfsr & (1U << 1))  { early_uart_puts("  IACCVIOL (instruction access)\r\n"); }
    if (cfsr & (1U << 0))  { early_uart_puts("  DACCVIOL (data access)\r\n"); }
    if (hfsr & (1U << 30)) { early_uart_puts("  FORCED (escalated)\r\n"); }
    if (hfsr & (1U << 1))  { early_uart_puts("  VECTBL (vector table read)\r\n"); }

    while (1) {
    }
}

void Reset_Handler(void)
{
    early_uart_putc('R');
    size_t *src = &__data_load__;
    size_t *dst = &__data_start__;

    __asm volatile("msr msplim, %0" : : "r"(&__StackLimit) : "memory");

    /* Copy .data from ROM to RAM */
    early_uart_putc('D');
    while (dst < &__data_end__) {
        *dst++ = *src++;
    }

    /* Copy .ramfunc if present */
    early_uart_putc('F');
    if (&__ramfunc_start__ != 0 && &__ramfunc_end__ != 0 && &__ramfunc_load__ != 0) {
        src = &__ramfunc_load__;
        dst = &__ramfunc_start__;
        while (dst < &__ramfunc_end__) {
            *dst++ = *src++;
        }
    }

    /* Zero .bss */
    early_uart_putc('B');
    dst = &__bss_start__;
    while (dst < &__bss_end__) {
        *dst++ = 0;
    }

    /* Zero .tbss */
    early_uart_putc('T');
    dst = (size_t *)&__tbss_start;
    while (dst < (size_t *)&__tbss_end) {
        *dst++ = 0;
    }

    early_uart_putc('L');
    _init_tls(__tls_base);
    _set_tls(__tls_base);

    /* Call C++ static constructors (init_array). */
    early_uart_putc('C');
    extern void (*__init_array_start[])(void);
    extern void (*__init_array_end[])(void);
    for (void (**p)(void) = __init_array_start; p < __init_array_end; p++) {
        (*p)();
    }

    early_uart_putc('S');
    SystemInit();
    __asm volatile("cpsie i" ::: "memory");
    early_uart_putc('M');
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
void LCDC1_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));

#define EXT_IRQ(n) [16 + (n)] = (uintptr_t)Default_Handler
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
    EXT_IRQ(0),  EXT_IRQ(1),  EXT_IRQ(2),  EXT_IRQ(3),
    EXT_IRQ(4),  EXT_IRQ(5),  EXT_IRQ(6),  EXT_IRQ(7),
    EXT_IRQ(8),  EXT_IRQ(9),  EXT_IRQ(10), EXT_IRQ(11),
    EXT_IRQ(12), EXT_IRQ(13), EXT_IRQ(14), EXT_IRQ(15),
    EXT_IRQ(16), EXT_IRQ(17), EXT_IRQ(18), EXT_IRQ(19),
    EXT_IRQ(20), EXT_IRQ(21), EXT_IRQ(22), EXT_IRQ(23),
    EXT_IRQ(24), EXT_IRQ(25), EXT_IRQ(26), EXT_IRQ(27),
    EXT_IRQ(28), EXT_IRQ(29), EXT_IRQ(30), EXT_IRQ(31),
    EXT_IRQ(32), EXT_IRQ(33), EXT_IRQ(34), EXT_IRQ(35),
    EXT_IRQ(36), EXT_IRQ(37), EXT_IRQ(38), EXT_IRQ(39),
    EXT_IRQ(40), EXT_IRQ(41), EXT_IRQ(42), EXT_IRQ(43),
    EXT_IRQ(44), EXT_IRQ(45), EXT_IRQ(46), EXT_IRQ(47),
    EXT_IRQ(48), EXT_IRQ(49), EXT_IRQ(50), EXT_IRQ(51),
    EXT_IRQ(52), EXT_IRQ(53), EXT_IRQ(54), EXT_IRQ(55),
    EXT_IRQ(56), EXT_IRQ(57), EXT_IRQ(58), EXT_IRQ(59),
    EXT_IRQ(60), EXT_IRQ(61), EXT_IRQ(62),
    [16 + 63] = (uintptr_t)LCDC1_IRQHandler,
};
