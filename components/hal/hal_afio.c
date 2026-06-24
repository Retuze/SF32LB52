#include "hal_afio.h"

void afio_config_pad(uint32_t pad, uint32_t fsel, uint32_t flags)
{
    sf32lb52_pinmux_config_pad(pad, fsel, flags);
}
