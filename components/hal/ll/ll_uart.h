#pragma once
#ifndef LL_UART_H
#define LL_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sf32lb52_uart1_write_byte(uint8_t value);
void sf32lb52_uart1_wait_tc(void);

#ifdef __cplusplus
}
#endif

#endif
