#pragma once
#ifndef LL_ATIM_H
#define LL_ATIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SF32LB52x ATIM1 direct-register PWM support. This is not a common driver. */

#define SF32_ATIM_OK                 0
#define SF32_ATIM_ERR_BAD_CHANNEL   -1
#define SF32_ATIM_ERR_BAD_ARGUMENT  -2
#define SF32_ATIM_ERR_CLOCK         -3

#define SF32_ATIM_CHANNEL_1 1U
#define SF32_ATIM_CHANNEL_2 2U
#define SF32_ATIM_CHANNEL_3 3U
#define SF32_ATIM_CHANNEL_4 4U

int sf32lb52_atim_pwm_configure(uint8_t channel, uint32_t frequency_hz);
int sf32lb52_atim_pwm_attach_pin(uint32_t pin, uint8_t channel);
int sf32lb52_atim_pwm_write_raw(uint8_t channel, uint32_t pulse_ticks);
uint32_t sf32lb52_atim_pwm_get_period_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
