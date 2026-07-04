#pragma once
#ifndef BB_UART_H
#define BB_UART_H

#include <stdint.h>

typedef void (*bb_delay_fn)(void);

typedef struct {
    uint32_t    pin_tx;
    uint32_t    pin_rx;
    bb_delay_fn bit_delay;
} bb_uart_t;

void    bb_uart_init(const bb_uart_t *dev);
void    bb_uart_send_byte(const bb_uart_t *dev, uint8_t data);
void    bb_uart_send(const bb_uart_t *dev, const uint8_t *data, uint16_t len);
uint8_t bb_uart_recv_byte(const bb_uart_t *dev);

#endif
