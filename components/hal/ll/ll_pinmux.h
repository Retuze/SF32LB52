#pragma once
#ifndef LL_PINMUX_H
#define LL_PINMUX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SF32_PINMUX_FSEL_Pos            0U
#define SF32_PINMUX_FSEL_Msk            (0xFUL << SF32_PINMUX_FSEL_Pos)
#define SF32_PINMUX_PULL_ENABLE         (1UL << 4)
#define SF32_PINMUX_PULL_SELECT_UP      (1UL << 5)
#define SF32_PINMUX_INPUT_ENABLE        (1UL << 6)
#define SF32_PINMUX_INPUT_SCHMITT       (1UL << 7)
#define SF32_PINMUX_SLEW_SLOW           (1UL << 8)
#define SF32_PINMUX_DRIVE_Pos           9U
#define SF32_PINMUX_DRIVE_Msk           (0x3UL << SF32_PINMUX_DRIVE_Pos)

#define SF32_PINMUX_PULL_NONE           0UL
#define SF32_PINMUX_PULL_DOWN           (SF32_PINMUX_PULL_ENABLE)
#define SF32_PINMUX_PULL_UP             (SF32_PINMUX_PULL_ENABLE | SF32_PINMUX_PULL_SELECT_UP)

#define SF32_PINMUX_DRIVE_0             (0UL << SF32_PINMUX_DRIVE_Pos)
#define SF32_PINMUX_DRIVE_1             (1UL << SF32_PINMUX_DRIVE_Pos)
#define SF32_PINMUX_DRIVE_2             (2UL << SF32_PINMUX_DRIVE_Pos)
#define SF32_PINMUX_DRIVE_3             (3UL << SF32_PINMUX_DRIVE_Pos)

void sf32lb52_pinmux_enable_clock(void);
void sf32lb52_pinmux_config_pad(uint32_t pad, uint32_t fsel, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
