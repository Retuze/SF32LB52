#include "ll_dwt.h"
#include "SF32LB52.h"

#define SF32_DWT_CTRL_ADDR   (0xE0001000UL)
#define SF32_DWT_CYCCNT_ADDR (0xE0001004UL)
#define SF32_DEMCR_ADDR      (0xE000EDFCUL)

#define DWT_CTRL_CYCCNTENA (1UL << 0)
#define DEMCR_TRCENA       (1UL << 24)

#define HPSYS_RCC_CSR_SEL_SYS_Pos     0U
#define HPSYS_RCC_CSR_SEL_SYS_Msk     (0x3UL << HPSYS_RCC_CSR_SEL_SYS_Pos)
#define HPSYS_RCC_CFGR_HDIV_Pos       0U
#define HPSYS_RCC_CFGR_HDIV_Msk       (0xFFUL << HPSYS_RCC_CFGR_HDIV_Pos)
#define HPSYS_RCC_DLL1CR_EN           (1UL << 0)
#define HPSYS_RCC_DLL1CR_STG_Pos      2U
#define HPSYS_RCC_DLL1CR_STG_Msk      (0xFUL << HPSYS_RCC_DLL1CR_STG_Pos)
#define HPSYS_RCC_DLL1CR_OUT_DIV2_EN  (1UL << 13)

#define SF32_DWT_HXT48_HZ             48000000UL

#define SF32_DWT_CTRL_REG   (*(volatile uint32_t *)SF32_DWT_CTRL_ADDR)
#define SF32_DWT_CYCCNT_REG (*(volatile uint32_t *)SF32_DWT_CYCCNT_ADDR)
#define SF32_DEMCR_REG      (*(volatile uint32_t *)SF32_DEMCR_ADDR)

static uint32_t sf32_dwt_get_hclk_hz_from_regs(void)
{
    uint32_t sel_sys = (HPSYS_RCC->CSR.R & HPSYS_RCC_CSR_SEL_SYS_Msk) >>
                       HPSYS_RCC_CSR_SEL_SYS_Pos;
    uint32_t sysclk_hz;
    uint32_t hdiv;

    if (sel_sys == 3U) {
        uint32_t dll1cr = HPSYS_RCC->DLL1CR.R;
        uint32_t stg;

        if ((dll1cr & HPSYS_RCC_DLL1CR_EN) == 0U) {
            return 0U;
        }

        stg = (dll1cr & HPSYS_RCC_DLL1CR_STG_Msk) >> HPSYS_RCC_DLL1CR_STG_Pos;
        sysclk_hz = (stg + 1U) * 24000000UL;
        if ((dll1cr & HPSYS_RCC_DLL1CR_OUT_DIV2_EN) != 0U) {
            sysclk_hz /= 2UL;
        }
    } else {
        sysclk_hz = SF32_DWT_HXT48_HZ;
    }

    hdiv = (HPSYS_RCC->CFGR.R & HPSYS_RCC_CFGR_HDIV_Msk) >> HPSYS_RCC_CFGR_HDIV_Pos;
    if (hdiv == 0U) {
        hdiv = 1U;
    }

    return sysclk_hz / hdiv;
}

static uint32_t sf32_dwt_us_to_cycles(uint32_t us)
{
    uint32_t hclk_hz = sf32_dwt_get_hclk_hz_from_regs();

    if (hclk_hz == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)us * (uint64_t)hclk_hz) / 1000000ULL);
}

static uint32_t sf32_dwt_cycles_to_us(uint32_t cycles)
{
    uint32_t hclk_hz = sf32_dwt_get_hclk_hz_from_regs();

    if (hclk_hz == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)hclk_hz);
}

static int dwt_init_impl(void)
{
    SF32_DEMCR_REG |= DEMCR_TRCENA;
    SF32_DWT_CYCCNT_REG = 0U;
    SF32_DWT_CTRL_REG |= DWT_CTRL_CYCCNTENA;

    if ((SF32_DWT_CTRL_REG & DWT_CTRL_CYCCNTENA) == 0U) {
        return -1;
    }

    return 0;
}

void dwt_set_us(uint32_t us)
{
    SF32_DWT_CYCCNT_REG = sf32_dwt_us_to_cycles(us);
}

uint32_t dwt_get_us(void)
{
    return sf32_dwt_cycles_to_us(SF32_DWT_CYCCNT_REG);
}

void dwt_init(void)
{
    (void)dwt_init_impl();
}
