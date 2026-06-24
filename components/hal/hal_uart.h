/**
 * @file hal_uart.h
 * @brief UART HAL — init + blocking TX
 */

#pragma once
#ifndef _SF32_UART_H_
#define _SF32_UART_H_

#include <stdint.h>

void uart_init(void);
void uart_write_byte(uint8_t value);
void uart_wait_tc(void);

#endif
