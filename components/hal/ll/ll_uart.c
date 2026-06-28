/**
 * @file ll_uart.c
 * @brief USART1 polled TX — putc + flush.
 *
 * USART1 is pre-configured by the ROM bootloader
 * (TX=pad19/PA06, RX=pad18/PA05, 1 Mbps). No init needed.
 */

#include "ll_uart.h"
#include "SF32LB52.h"

#define USART_ISR_TXE     (1UL << 7)
#define USART_ISR_TC      (1UL << 6)

void uart_putc(uint8_t c)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0U) {}
    USART1->TDR = c;
}

void uart_flush(void)
{
    while ((USART1->ISR & USART_ISR_TC) == 0U) {}
}
