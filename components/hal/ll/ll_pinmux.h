#pragma once
#ifndef PINMUX_H
#define PINMUX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pinmux.h
 * @brief Pad multiplexing — function select + electrical config.
 *
 *     pinmux_config(pad, fsel, PINMUX_PULL_UP | PINMUX_DRIVE_3);
 */

/* ── Register bit definitions ──────────────────────────────────────────── */

#define PINMUX_FSEL_Pos       0U
#define PINMUX_FSEL_Msk       (0xFUL << PINMUX_FSEL_Pos)
#define PINMUX_PULL_ENABLE    (1UL << 4)
#define PINMUX_PULL_UP_SEL    (1UL << 5)
#define PINMUX_INPUT_ENABLE   (1UL << 6)
#define PINMUX_INPUT_SCHMITT  (1UL << 7)
#define PINMUX_SLEW_SLOW      (1UL << 8)
#define PINMUX_DRIVE_Pos      9U
#define PINMUX_DRIVE_Msk      (0x3UL << PINMUX_DRIVE_Pos)

/* ── Composed values ───────────────────────────────────────────────────── */

#define PINMUX_PULL_NONE      0UL
#define PINMUX_PULL_DOWN      (PINMUX_PULL_ENABLE)
#define PINMUX_PULL_UP        (PINMUX_PULL_ENABLE | PINMUX_PULL_UP_SEL)

#define PINMUX_DRIVE_0        (0UL << PINMUX_DRIVE_Pos)
#define PINMUX_DRIVE_1        (1UL << PINMUX_DRIVE_Pos)
#define PINMUX_DRIVE_2        (2UL << PINMUX_DRIVE_Pos)
#define PINMUX_DRIVE_3        (3UL << PINMUX_DRIVE_Pos)

/* ── API ───────────────────────────────────────────────────────────────── */

void pinmux_clk_enable(void);
void pinmux_config(uint32_t pad, uint32_t fsel, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* PINMUX_H */
