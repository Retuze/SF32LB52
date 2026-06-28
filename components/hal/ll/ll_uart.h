#pragma once
#ifndef LL_UART_H
#define LL_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ll_uart.h
 * @brief USART1 polled driver — putc + flush.
 *
 * No init needed: the ROM bootloader already configures USART1
 * (TX=pad19/PA06, RX=pad18/PA05, 1 Mbps).
 */

void uart_putc(uint8_t c);
void uart_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* LL_UART_H */
