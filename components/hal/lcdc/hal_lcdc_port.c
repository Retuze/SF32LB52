#include "bf0_hal.h"

uint32_t HAL_GetTick(void)
{
    return millis();
}

void HAL_Delay_us(uint32_t us)
{
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = us * 240U;
    while ((uint32_t)(DWT_CYCCNT - start) < cycles) {
    }
}

uint32_t HAL_RCC_GetHCLKFreq(uint32_t core_id)
{
    (void)core_id;
    return 240000000U;
}

void HAL_RCC_EnableModule(uint32_t module)
{
    if (module == RCC_MOD_LCDC1) {
        HPSYS_RCC->ENR1.R |= (1UL << 7);
        HPSYS_RCC->ESR1.R |= (1UL << 7);
    }
}

void HAL_RCC_ResetModule(uint32_t module)
{
    if (module == RCC_MOD_LCDC1) {
        HPSYS_RCC->RSTR1.R |= (1UL << 7);
        for (volatile uint32_t i = 0; i < 64U; ++i) {
        }
        HPSYS_RCC->RSTR1.R &= ~(1UL << 7);
    }
}
