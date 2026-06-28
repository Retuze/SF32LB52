/**
 * @file clock.c
 * @brief System clock control — HCLK query, DVFS, DLL frequency switch.
 */

#include "clock.h"
#include "SF32LB52.h"

/* ── RCC register fields ──────────────────────────────────────────────── */

#define CSR_SEL_SYS_Pos   0U
#define CSR_SEL_SYS_Msk   (0x3UL << CSR_SEL_SYS_Pos)

#define CFGR_HDIV_Pos     0U
#define CFGR_HDIV_Msk     (0xFFUL << CFGR_HDIV_Pos)
#define CFGR_PDIV1_Pos    8U
#define CFGR_PDIV1_Msk    (0x7UL << CFGR_PDIV1_Pos)
#define CFGR_PDIV2_Pos    12U
#define CFGR_PDIV2_Msk    (0x7UL << CFGR_PDIV2_Pos)

#define DLL1CR_EN         (1UL << 0)
#define DLL1CR_STG_Pos    2U
#define DLL1CR_STG_Msk    (0xFUL << DLL1CR_STG_Pos)
#define DLL1CR_XTALIN_EN  (1UL << 6)
#define DLL1CR_IN_DIV2_EN (1UL << 12)
#define DLL1CR_OUT_DIV2_EN (1UL << 13)
#define DLL1CR_READY      (1UL << 31)

/* SysTick */
#define SYST_CSR_ENABLE     (1UL << 0)
#define SYST_CSR_TICKINT    (1UL << 1)
#define SYST_CSR_CLKSOURCE  (1UL << 2)

/* PMUC HXT */
#define HXT_CR1_EN         (1UL << 0)
#define HXT_CR1_BUF_EN     (1UL << 1)
#define HXT_CR1_BUF_DIG_EN (1UL << 2)
#define HXT_CR1_BUF_DLL_EN (1UL << 5)
#define HXT_CR1_GM_EN      (1UL << 19)

/* ── DVFS tables ──────────────────────────────────────────────────────── */

typedef enum {
    DVFS_D0, DVFS_D1, DVFS_S0, DVFS_S1
} dvfs_mode_t;

typedef struct {
    int8_t   ldo_off;
    uint8_t  ldo;
    uint8_t  buck;
    uint32_t ulpmcr;
} dvfs_cfg_t;

static const dvfs_cfg_t dvfs_table[] = {
    [DVFS_D0] = { -5, 0x6, 0x9, 0x00100330UL },
    [DVFS_D1] = { -3, 0x8, 0xA, 0x00110331UL },
    [DVFS_S0] = {  0, 0xB, 0xD, 0x00130213UL },
    [DVFS_S1] = {  2, 0xD, 0xF, 0x00130213UL },
};

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void delay_cycles(volatile uint32_t n)
{
    while (n--) { __asm volatile("nop"); }
}

static dvfs_mode_t dvfs_pick(uint32_t target_hz)
{
    if (target_hz > 144000000UL) return DVFS_S1;
    if (target_hz > HCLK_48MHZ)  return DVFS_S0;
    if (target_hz > 24000000UL)  return DVFS_D1;
    return DVFS_D0;
}

static void dvfs_apply(uint32_t target_hz)
{
    dvfs_mode_t m = dvfs_pick(target_hz);
    const dvfs_cfg_t *c = &dvfs_table[m];

    if (m >= DVFS_S0) {
        MODIFY_REG(PMUC->BUCK_VOUT, PMUC_BUCK_VOUT_VOUT_Msk,
                   ((uint32_t)c->buck << PMUC_BUCK_VOUT_VOUT_Pos) & PMUC_BUCK_VOUT_VOUT_Msk);
        MODIFY_REG(PMUC->HPSYS_VOUT, PMUC_HPSYS_VOUT_VOUT_Msk,
                   ((uint32_t)c->ldo << PMUC_HPSYS_VOUT_VOUT_Pos) & PMUC_HPSYS_VOUT_VOUT_Msk);
        CLEAR_BIT(HPSYS_CFG->SYSCR, HPSYS_CFG_SYSCR_LDO_VSEL_Msk);
        HPSYS_CFG->ULPMCR = c->ulpmcr;
        delay_cycles(20000UL);
    } else {
        int32_t vref = (int32_t)dvfs_table[DVFS_S0].ldo + (int32_t)c->ldo_off;
        if (vref < 0) vref = 0;
        MODIFY_REG(PMUC->BUCK_CR2, PMUC_BUCK_CR2_SET_VOUT_M_Msk,
                   ((uint32_t)c->buck << PMUC_BUCK_CR2_SET_VOUT_M_Pos) & PMUC_BUCK_CR2_SET_VOUT_M_Msk);
        MODIFY_REG(PMUC->HPSYS_LDO, PMUC_HPSYS_LDO_VREF_Msk,
                   ((uint32_t)vref << PMUC_HPSYS_LDO_VREF_Pos) & PMUC_HPSYS_LDO_VREF_Msk);
        HPSYS_CFG->ULPMCR = c->ulpmcr;
        SET_BIT(HPSYS_CFG->SYSCR, HPSYS_CFG_SYSCR_LDO_VSEL_Msk);
    }
}

/* Update SysTick for the new HCLK (1 ms tick) */
static void systick_update(uint32_t hclk_hz)
{
    if (hclk_hz == 0U) hclk_hz = HCLK_48MHZ;
    SYST_CSR = 0U;
    SYST_RVR = (hclk_hz / 1000UL) - 1UL;
    SYST_CVR = 0U;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

/* ── Public API ───────────────────────────────────────────────────────── */

uint32_t clk_get_hz(void)
{
    uint32_t sel = (HPSYS_RCC->CSR.R & CSR_SEL_SYS_Msk) >> CSR_SEL_SYS_Pos;
    uint32_t sysclk;

    if (sel == 3U) {
        uint32_t dll = HPSYS_RCC->DLL1CR.R;
        if ((dll & DLL1CR_EN) == 0U) return 0U;
        uint32_t stg = (dll & DLL1CR_STG_Msk) >> DLL1CR_STG_Pos;
        sysclk = (stg + 1U) * 24000000UL;
        if (dll & DLL1CR_OUT_DIV2_EN) sysclk /= 2UL;
    } else {
        sysclk = HCLK_48MHZ;
    }

    uint32_t hdiv = (HPSYS_RCC->CFGR.R & CFGR_HDIV_Msk) >> CFGR_HDIV_Pos;
    if (hdiv == 0U) hdiv = 1U;
    return sysclk / hdiv;
}

void clk_set_hz(uint32_t target_hz)
{
    uint32_t stg, val, timeout;

    /* 48 MHz: switch to HXT directly */
    if (target_hz == HCLK_48MHZ) {
        MODIFY_REG(HPSYS_RCC->CSR.R, CSR_SEL_SYS_Msk, 1UL << CSR_SEL_SYS_Pos);
        MODIFY_REG(HPSYS_RCC->CFGR.R, CFGR_HDIV_Msk, 1UL << CFGR_HDIV_Pos);
        systick_update(clk_get_hz());
        return;
    }

    /* Validate: must be multiple of 24 MHz in [72, 240] */
    if (target_hz < 72000000UL || target_hz > HCLK_MAX || (target_hz % 24000000UL)) return;

    dvfs_apply(target_hz);

    stg = (target_hz / 24000000UL) - 1UL;

    /* Enable HXT oscillator + buffers */
    SET_BIT(PMUC->HXT_CR1, HXT_CR1_EN | HXT_CR1_BUF_EN |
                            HXT_CR1_BUF_DIG_EN | HXT_CR1_BUF_DLL_EN | HXT_CR1_GM_EN);

    /* Switch to HXT (48 MHz) before DLL re-lock */
    MODIFY_REG(HPSYS_RCC->CSR.R, CSR_SEL_SYS_Msk, 1UL << CSR_SEL_SYS_Pos);

    SET_BIT(HPSYS_CFG->CAU2_CR, HPSYS_CFG_CAU2_CR_HPBG_EN_Msk |
                                 HPSYS_CFG_CAU2_CR_HPBG_VDDPSW_EN_Msk);

    /* Configure DLL: 24 MHz → target */
    val  = HPSYS_RCC->DLL1CR.R;
    val &= ~(DLL1CR_EN | DLL1CR_STG_Msk | DLL1CR_OUT_DIV2_EN);
    val |= DLL1CR_XTALIN_EN | DLL1CR_IN_DIV2_EN | (stg << DLL1CR_STG_Pos);
    HPSYS_RCC->DLL1CR.R = val;
    HPSYS_RCC->DLL1CR.R = val | DLL1CR_EN;

    /* Wait for DLL lock */
    timeout = 1000000UL;
    while (!(HPSYS_RCC->DLL1CR.R & DLL1CR_READY) && --timeout) {}
    if (timeout == 0U) return;

    /* Set dividers and switch to DLL */
    MODIFY_REG(HPSYS_RCC->CFGR.R,
               CFGR_HDIV_Msk | CFGR_PDIV1_Msk | CFGR_PDIV2_Msk,
               (1UL << CFGR_HDIV_Pos) | (1UL << CFGR_PDIV1_Pos) | (6UL << CFGR_PDIV2_Pos));
    MODIFY_REG(HPSYS_RCC->CSR.R, CSR_SEL_SYS_Msk, 3UL << CSR_SEL_SYS_Pos);

    systick_update(clk_get_hz());
}
