#pragma once
#ifndef _SF32_AFIO_H_
#define _SF32_AFIO_H_

#include <stdint.h>
#include "ll_pinmux.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SF32_AFIO_FSEL_Pos            SF32_PINMUX_FSEL_Pos
#define SF32_AFIO_FSEL_Msk            SF32_PINMUX_FSEL_Msk
#define SF32_AFIO_PULL_ENABLE         SF32_PINMUX_PULL_ENABLE
#define SF32_AFIO_PULL_SELECT_UP      SF32_PINMUX_PULL_SELECT_UP
#define SF32_AFIO_INPUT_ENABLE        SF32_PINMUX_INPUT_ENABLE
#define SF32_AFIO_INPUT_SCHMITT       SF32_PINMUX_INPUT_SCHMITT
#define SF32_AFIO_SLEW_SLOW           SF32_PINMUX_SLEW_SLOW
#define SF32_AFIO_DRIVE_Pos           SF32_PINMUX_DRIVE_Pos
#define SF32_AFIO_DRIVE_Msk           SF32_PINMUX_DRIVE_Msk

#define SF32_AFIO_PULL_NONE           SF32_PINMUX_PULL_NONE
#define SF32_AFIO_PULL_DOWN           SF32_PINMUX_PULL_DOWN
#define SF32_AFIO_PULL_UP             SF32_PINMUX_PULL_UP

#define SF32_AFIO_DRIVE_0             SF32_PINMUX_DRIVE_0
#define SF32_AFIO_DRIVE_1             SF32_PINMUX_DRIVE_1
#define SF32_AFIO_DRIVE_2             SF32_PINMUX_DRIVE_2
#define SF32_AFIO_DRIVE_3             SF32_PINMUX_DRIVE_3

void afio_config_pad(uint32_t pad, uint32_t fsel, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
