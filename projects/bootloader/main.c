/**
 * @file main.c
 * @brief Bootloader — minimal init, OTA check, jump to firmware
 */

#include <stdint.h>
#include "hal.h"
#include "board.h"
#include "SF32LB52.h"

extern uint32_t APPLICATION_ADDR;

typedef void (*app_entry_t)(void);

/* ---- Direct UART TX (no init — ROM bootloader already configured USART1) */
#define USART_ISR_TXE  (1UL << 7)
#define USART_ISR_TC   (1UL << 6)

static void uart_putc(char c)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0U) {}
    USART1->TDR = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) { uart_putc(*s++); }
    while ((USART1->ISR & USART_ISR_TC) == 0U) {}
}

__attribute__((noreturn)) static void jump_to_app(uint32_t addr)
{
    uint32_t *vt = (uint32_t *)addr;
    uint32_t app_sp = vt[0];
    uint32_t app_reset = vt[1] | 1U;

    __asm volatile("cpsid i" ::: "memory");
    SYST_CSR = 0U;
    SCB_VTOR = addr;
    __asm volatile("dsb");
    __asm volatile("isb");
    __asm volatile("cpsie i" ::: "memory");
    __asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r"(app_sp), "r"(app_reset)
        : "memory");

    __builtin_unreachable();
}

int main(void)
{
    /* Step 1: Prove the bootloader runs — just use ROM's UART config.
     * Try different terminal baud rates: 1M, 115200, 9600, etc.
     * Whatever produces readable text is the ROM's baud rate. */
    uart_puts("\r\n[BOOT] Hello from bootloader!\r\n");

    /* Step 2: Init clock to 240 MHz */
    uart_puts("[BOOT] Setting clock to 240MHz...\r\n");
    rcc_set_system_hz(240000000UL);

    /* Step 3: Keep ROM bootloader's UART1 1 Mbps configuration */
    uart_puts("[BOOT] Clock done, keeping ROM UART1 config\r\n");

    /* Step 4: Init LED */
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, HIGH);

    uart_puts("[BOOT] Jumping to firmware...\r\n");
    uart_putc('X');  /* flush */

    /* Blink 3 times */
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_RED, LOW);
        for (volatile int d = 0; d < 1000000; d++) {}
        digitalWrite(LED_RED, HIGH);
        for (volatile int d = 0; d < 1000000; d++) {}
    }

    jump_to_app((uint32_t)(uintptr_t)&APPLICATION_ADDR);

    while (1) {}
}
