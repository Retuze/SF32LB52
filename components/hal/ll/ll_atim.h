#pragma once
#ifndef ATIM_H
#define ATIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file atim.h
 * @brief ATIM1 PWM — configure, attach pin, write duty.
 *
 *     atim_pwm_init(ch, hz);
 *     atim_pwm_pin(pin, ch);
 *     atim_pwm_write(ch, ticks);
 */

#define ATIM_CH1  1U
#define ATIM_CH2  2U
#define ATIM_CH3  3U
#define ATIM_CH4  4U

int      atim_pwm_init(uint8_t channel, uint32_t freq_hz);
int      atim_pwm_pin(uint32_t pin, uint8_t channel);
int      atim_pwm_write(uint8_t channel, uint32_t ticks);
uint32_t atim_pwm_period(void);

#ifdef __cplusplus
}
#endif

#endif /* ATIM_H */
