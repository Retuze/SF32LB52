#pragma once
#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PWM_ATIM_CHANNEL_1 1U
#define PWM_ATIM_CHANNEL_2 2U
#define PWM_ATIM_CHANNEL_3 3U
#define PWM_ATIM_CHANNEL_4 4U

void analogWriteFrequency(uint32_t frequency_hz);
void analogWriteResolution(uint8_t bits);
int analogWrite(uint32_t pin, uint32_t value);
int pwmAttachPinToAtim(uint32_t pin, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif
