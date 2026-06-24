#include "ll_pinmux.h"

#include "SF32LB52.h"

void sf32lb52_pinmux_enable_clock(void)
{
    HPSYS_RCC->ENR1.R |= HPSYS_RCC_ENR1_PINMUX1_Msk;
    HPSYS_RCC->ESR1.R |= HPSYS_RCC_ESR1_PINMUX1_Msk;
}

void sf32lb52_pinmux_config_pad(uint32_t pad, uint32_t fsel, uint32_t flags)
{
    uint32_t cfg;

    if (pad >= HPSYS_PINMUX_PAD_COUNT) {
        return;
    }

    sf32lb52_pinmux_enable_clock();

    cfg = flags & (SF32_PINMUX_PULL_ENABLE | SF32_PINMUX_PULL_SELECT_UP |
                   SF32_PINMUX_INPUT_ENABLE | SF32_PINMUX_INPUT_SCHMITT |
                   SF32_PINMUX_SLEW_SLOW | SF32_PINMUX_DRIVE_Msk);
    cfg |= (fsel << SF32_PINMUX_FSEL_Pos) & SF32_PINMUX_FSEL_Msk;

    HPSYS_PINMUX->PAD[pad].R = cfg;
}
