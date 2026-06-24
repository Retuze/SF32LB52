#include "ll_uart.h"

#include "SF32LB52.h"

#define USART_ISR_TC             (1UL << 6)
#define USART_ISR_TXE            (1UL << 7)

void sf32lb52_uart1_write_byte(uint8_t value)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0U) {
    }
    USART1->TDR = value;
}

void sf32lb52_uart1_wait_tc(void)
{
    while ((USART1->ISR & USART_ISR_TC) == 0U) {
    }
}
