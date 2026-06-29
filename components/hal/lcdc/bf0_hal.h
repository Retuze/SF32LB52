#pragma once
#include "hal.h"
#include "bf0_hal_def.h"
#include "register.h"

#define SOC_BF0_HCPU 1
#define SF32LB52X 1
#define HAL_LCD_MODULE_ENABLED 1

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif
#ifndef __HAL_ROM_USED
#define __HAL_ROM_USED
#endif

#define MAKE_REG_VAL(val, mask, offset) ((((uint32_t)(val)) << (offset)) & (mask))
#define GET_REG_VAL(reg, mask, offset) ((((uint32_t)(reg)) & (mask)) >> (offset))
#define HAL_ASSERT(expr) do { if (!(expr)) { while (1) { } } } while (0)

uint32_t HAL_GetTick(void);
void HAL_Delay_us(uint32_t us);
uint32_t HAL_RCC_GetHCLKFreq(uint32_t core_id);
void HAL_RCC_EnableModule(uint32_t module);
void HAL_RCC_ResetModule(uint32_t module);

#define RCC_MOD_LCDC1 0U
#define RCC_MOD_LCDC2 1U
#define HAL_CHIP_REV_ID_A3 2U
#define HAL_CHIP_REV_ID_A4 3U
#define HAL_CHIP_REV_ID_B4 4U
#define __HAL_SYSCFG_GET_REVID() (HAL_CHIP_REV_ID_A4)

#include "bf0_hal_lcdc.h"
