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

static void uart_puts(const char *s)
{
    while (*s) { uart_putc(*s++); }
    uart_flush();
}

static void uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(hex[(v >> i) & 0xFU]);
    }
}

static void dump_words(const char *tag, const uint32_t *p, int count)
{
    uart_puts(tag);
    for (int i = 0; i < count; i++) {
        uart_puts(" ");
        uart_put_hex32(p[i]);
    }
    uart_puts("\r\n");
}

__attribute__((noreturn)) static void jump_to_app(uint32_t addr)
{
    uint32_t *vt = (uint32_t *)addr;
    uint32_t app_sp = vt[0];
    uint32_t app_reset = vt[1] | 1U;

    uart_puts("[BOOT] app vector:");
    dump_words("", vt, 4);
    uart_puts("[BOOT] app SP=");
    uart_put_hex32(app_sp);
    uart_puts(" reset=");
    uart_put_hex32(app_reset);
    uart_puts("\r\n");

    if ((app_sp == 0xCCCCCCCCU) || (app_reset == 0xCCCCCCCDU)) {
        uart_puts("[BOOT] invalid app vector, halt\r\n");
        while (1) {}
    }

    __asm volatile("cpsid i" ::: "memory");
    SYST_CSR = 0U;
    SCB_VTOR = addr;
    __asm volatile("dsb");
    __asm volatile("isb");
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
    clk_set_hz(HCLK_240MHZ);

    dump_words("[BOOT] app before init:", (const uint32_t *)(uintptr_t)&APPLICATION_ADDR, 4);

    /* Step 2.5: Enable DLL2 + configure MPI2 Flash for quad QSPI (RAM-resident). */
    uart_puts("[BOOT] Configuring Flash Quad QSPI...\r\n");
    flash_quad_init();
    dump_words("[BOOT] app after init:", (const uint32_t *)(uintptr_t)&APPLICATION_ADDR, 4);
    uart_puts("[BOOT] Flash Quad QSPI ready\r\n");

    /* Step 3: Init LED */
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
