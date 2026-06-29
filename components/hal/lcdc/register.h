#pragma once
#include "SF32LB52.h"
#include "lcd_if.h"

#define SOC_BF0_HCPU 1
#define SF32LB52X 1
#define CORE_ID_HCPU 0U
#define CORE_ID_LCPU 1U

#define LCDC1_BASE 0x50008000UL
#define hwp_lcdc1 ((LCD_IF_TypeDef *)LCDC1_BASE)
#define LCDC1 hwp_lcdc1

#define HCPU_IS_MPI_CBUS_ADDR(addr) ((((uint32_t)(addr)) >= 0x10000000UL) && (((uint32_t)(addr)) < 0x20000000UL))
#define HCPU_MPI_CBUS_ADDR_2_SBUS_ADDR(addr) ((uint32_t)(addr) + 0x50000000UL)
#define HCPU_MPI_SBUS_ADDR_2_CBUS_ADDR(addr) ((uint32_t)(addr) - 0x50000000UL)
#define HCPU_MPI_SBUS_ADDR(addr) (HCPU_IS_MPI_CBUS_ADDR(addr) ? HCPU_MPI_CBUS_ADDR_2_SBUS_ADDR(addr) : ((uint32_t)(addr)))

typedef enum {
    LCDC1_IRQn = 63,
} IRQn_Type;
