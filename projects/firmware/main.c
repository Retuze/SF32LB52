/**
 * @file main.c
 * @brief Watch firmware — matched to the verified-working lcd_test project.
 *
 * Uses the reference port (lcd_ref) for LCD.  UART uses the ROM bootloader's
 * USART1 configuration — no uart_init() call, same as the reference project.
 */

#include <stdint.h>
#include "hal.h"
#include "board.h"
#include "lcd_ref.h"

#define USART_ISR_TXE  (1UL << 7)
#define USART_ISR_TC   (1UL << 6)

/* ---- Polled UART (same as bootloader, uses ROM's USART1 config) -------- */
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

static void uart_print_hex(unsigned long v)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(v >> shift) & 0xFUL]);
    }
}

static void uart_print_dec(unsigned long v)
{
    char buf[10];
    int i = 0;
    if (v == 0UL) { uart_putc('0'); return; }
    while (v > 0UL) { buf[i++] = '0' + (char)(v % 10UL); v /= 10UL; }
    while (i > 0) { uart_putc(buf[--i]); }
}

/* ---- main -------------------------------------------------------------- */
int main(void)
{
    uint16_t color = 0x0000U;
    uint32_t start_cyc, elapsed_us;

    uart_puts("\r\n[firmware] entered main\r\n");

    /* ---- Set system clock to 240 MHz ---- */
    rcc_set_system_hz(240000000UL);
    uart_puts("[firmware] clock set to 240 MHz\r\n");

    /* ---- Init onboard LED ---- */
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, HIGH);
    delay(200);
    digitalWrite(LED_RED, LOW);

    uart_puts("[firmware] Watch V1.0 booted\r\n");

    /* ---- LCD init (reference port, same as lcd_test) ---- */
    uart_puts("[firmware] LCD init...\r\n");
    lcd_ref_init();
    uart_puts("[firmware] LCD init done\r\n");

    /* ---- Main loop: flash black/white ---- */
    uart_puts("[firmware] Entering main loop\r\n");
    while (1) {
        start_cyc = DWT_CYCCNT;
        lcd_ref_fill_rect(0U, 0U,
                          LCD_WIDTH_REF - 1U, LCD_HEIGHT_REF - 1U,
                          color);
        elapsed_us = (DWT_CYCCNT - start_cyc) / 240UL;  /* 240 cycles/us at 240 MHz */
        uart_puts("fill: ");
        uart_print_dec(elapsed_us);
        uart_puts(" us\r\n");

        color = (color == 0x0000U) ? 0xFFFFU : 0x0000U;
        digitalToggle(LED_RED);
        delay(500);
    }
}
