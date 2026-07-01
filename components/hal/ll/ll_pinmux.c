/**
 * @file pinmux.c
 * @brief Pad configuration — function select, pull, drive, input mode.
 */

#include "ll_pinmux.h"
#include "SF32LB52.h"

void pinmux_clk_enable(void)
{
    HPSYS_RCC->ENR1.R |= HPSYS_RCC_ENR1_PINMUX1_Msk;
    HPSYS_RCC->ESR1.R |= HPSYS_RCC_ESR1_PINMUX1_Msk;
}

void pinmux_config(uint32_t pad, uint32_t fsel, uint32_t flags)
{
    /* Logical PA pin → physical PAD index (skip 13 SA pads at PAD[0..12]) */
    uint32_t pidx = pad + PA_PAD_OFFSET;
    if (pidx >= HPSYS_PINMUX_PAD_COUNT) return;

    pinmux_clk_enable();

    uint32_t cfg  = flags & (PINMUX_PULL_ENABLE | PINMUX_PULL_UP_SEL |
                             PINMUX_INPUT_ENABLE | PINMUX_INPUT_SCHMITT |
                             PINMUX_SLEW_SLOW | PINMUX_DRIVE_Msk);
    cfg |= (fsel << PINMUX_FSEL_Pos) & PINMUX_FSEL_Msk;

    HPSYS_PINMUX->PAD[pidx].R = cfg;

    /* DSB + dummy read-back prevents adjacent-PAD glitch on SF32LB52 */
    __asm volatile("dsb" ::: "memory");
    (void)HPSYS_PINMUX->PAD[pidx].R;
}
