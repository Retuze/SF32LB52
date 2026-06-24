#pragma once

#include <stdint.h>

#define SF32_RCC_HCLK_HXT48_HZ 48000000UL
#define SF32_RCC_HCLK_MAX_HZ   240000000UL

#define SF32_RCC_OK                     0
#define SF32_RCC_ERR_INVALID_TARGET    -1
#define SF32_RCC_ERR_DLL1_LOCK_TIMEOUT -2
#define SF32_RCC_ERR_SWITCH_VERIFY     -3
#define SF32_RCC_ERR_DVFS              -4

void rcc_init(void);
void rcc_set_system_hz(uint32_t hz);
uint32_t rcc_get_system_hz(void);

extern volatile uint32_t uwTick;
