/**
 * @file hal_uart.c
 * @brief UART HAL — USART1 init + blocking TX via register-level access.
 *
 * Pin mapping (Watch V1.0 / DevKit Nano):
 *   TX  — pad 19 (PA06), FSEL=4
 *   RX  — pad 18 (PA05), FSEL=4
 *
 * USART1_PINR register routes TXD/RXD to physical pads.
 * FSEL=4 selects the I2C/UART alternate function on PA pads.
 */

#include "hal_uart.h"
#include "SF32LB52.h"
#include "ll_pinmux.h"
#include "ll_rcc.h"

/* ---- USART1 register bits (STM32-compatible subset) ------------------- */
#define USART_CR1_UE      (1UL << 0)   /* USART enable */
#define USART_CR1_RE      (1UL << 2)   /* Receiver enable */
#define USART_CR1_TE      (1UL << 3)   /* Transmitter enable */
#define USART_CR1_OVER8   (1UL << 15)  /* Oversampling mode (0=16, 1=8) */

#define USART_ISR_TXE     (1UL << 7)   /* TX data register empty */
#define USART_ISR_TC      (1UL << 6)   /* Transmission complete */

/* ---- USART1_PINR bit fields ------------------------------------------- */
#define USART1_PINR_TXD_Pos  0U
#define USART1_PINR_TXD_Msk  (0x3FUL << USART1_PINR_TXD_Pos)
#define USART1_PINR_RXD_Pos  8U
#define USART1_PINR_RXD_Msk  (0x3FUL << USART1_PINR_RXD_Pos)

/* ---- Pinmux flags for UART TX / RX ------------------------------------ */
#define UART_TX_FLAGS  (SF32_PINMUX_PULL_UP)
#define UART_RX_FLAGS  (SF32_PINMUX_PULL_UP | SF32_PINMUX_INPUT_ENABLE | SF32_PINMUX_INPUT_SCHMITT)
#define UART_FSEL      4U

/* ---- UART pad numbers ------------------------------------------------- */
#define UART_TX_PAD  19U
#define UART_RX_PAD  18U

void uart_init(void)
{
    /* 1. Enable pinmux clock */
    sf32lb52_pinmux_enable_clock();

    /* 2. Route USART1 TXD/RXD to pads 19/18 */
    uint32_t pinr = HPSYS_CFG->USART1_PINR;
    pinr &= ~(USART1_PINR_TXD_Msk | USART1_PINR_RXD_Msk);
    pinr |= (UART_TX_PAD << USART1_PINR_TXD_Pos);
    pinr |= (UART_RX_PAD << USART1_PINR_RXD_Pos);
    HPSYS_CFG->USART1_PINR = pinr;

    /* 3. Configure pads: FSEL=4 (I2C/UART), pull-up, input on RX */
    sf32lb52_pinmux_config_pad(UART_TX_PAD, UART_FSEL, UART_TX_FLAGS);
    sf32lb52_pinmux_config_pad(UART_RX_PAD, UART_FSEL, UART_RX_FLAGS);

    /* 4. Configure USART1: 1 Mbps (match ROM bootloader, BRR = HCLK/1M) */
    USART1->CR1 = 0U;
    USART1->BRR = 240000000UL / 1000000UL;       /* = 240 for 240 MHz */
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void uart_write_byte(uint8_t value)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0U) {
    }
    USART1->TDR = value;
}

void uart_wait_tc(void)
{
    while ((USART1->ISR & USART_ISR_TC) == 0U) {
    }
}
