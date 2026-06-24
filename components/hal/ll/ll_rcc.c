#include "ll_rcc.h"
#include "SF32LB52.h"

#define HPSYS_RCC_CSR_SEL_SYS_Pos     0U
#define HPSYS_RCC_CSR_SEL_SYS_Msk     (0x3UL << HPSYS_RCC_CSR_SEL_SYS_Pos)

#define HPSYS_RCC_CFGR_HDIV_Pos       0U
#define HPSYS_RCC_CFGR_HDIV_Msk       (0xFFUL << HPSYS_RCC_CFGR_HDIV_Pos)
#define HPSYS_RCC_CFGR_PDIV1_Pos      8U
#define HPSYS_RCC_CFGR_PDIV1_Msk      (0x7UL << HPSYS_RCC_CFGR_PDIV1_Pos)
#define HPSYS_RCC_CFGR_PDIV2_Pos      12U
#define HPSYS_RCC_CFGR_PDIV2_Msk      (0x7UL << HPSYS_RCC_CFGR_PDIV2_Pos)

#define HPSYS_RCC_DLL1CR_EN           (1UL << 0)
#define HPSYS_RCC_DLL1CR_STG_Pos      2U
#define HPSYS_RCC_DLL1CR_STG_Msk      (0xFUL << HPSYS_RCC_DLL1CR_STG_Pos)
#define HPSYS_RCC_DLL1CR_XTALIN_EN    (1UL << 6)
#define HPSYS_RCC_DLL1CR_IN_DIV2_EN   (1UL << 12)
#define HPSYS_RCC_DLL1CR_OUT_DIV2_EN  (1UL << 13)
#define HPSYS_RCC_DLL1CR_READY        (1UL << 31)

#define PMUC_HXT_CR1_EN               (1UL << 0)
#define PMUC_HXT_CR1_BUF_EN           (1UL << 1)
#define PMUC_HXT_CR1_BUF_DIG_EN       (1UL << 2)
#define PMUC_HXT_CR1_BUF_DLL_EN       (1UL << 5)
#define PMUC_HXT_CR1_GM_EN            (1UL << 19)

typedef enum {
    SF32_DVFS_MODE_D0 = 0,
    SF32_DVFS_MODE_D1,
    SF32_DVFS_MODE_S0,
    SF32_DVFS_MODE_S1,
} sf32_dvfs_mode_t;

typedef struct {
    int8_t ldo_offset;
    uint8_t ldo;
    uint8_t buck;
    uint32_t ulpmcr;
} sf32_dvfs_cfg_t;

static const sf32_dvfs_cfg_t g_sf32_dvfs_cfg[] = {
    [SF32_DVFS_MODE_D0] = {-5, 0x6, 0x9, 0x00100330UL},
    [SF32_DVFS_MODE_D1] = {-3, 0x8, 0xA, 0x00110331UL},
    [SF32_DVFS_MODE_S0] = {0, 0xB, 0xD, 0x00130213UL},
    [SF32_DVFS_MODE_S1] = {2, 0xD, 0xF, 0x00130213UL},
};

volatile uint32_t uwTick;

static void sf32_rcc_delay_cycles(volatile uint32_t cycles)
{
    while (cycles > 0U) {
        --cycles;
        __asm volatile ("nop");
    }
}

static sf32_dvfs_mode_t sf32_rcc_pick_dvfs_mode(uint32_t target_hz)
{
    if (target_hz > 144000000UL) {
        return SF32_DVFS_MODE_S1;
    }
    if (target_hz > SF32_RCC_HCLK_HXT48_HZ) {
        return SF32_DVFS_MODE_S0;
    }
    if (target_hz > 24000000UL) {
        return SF32_DVFS_MODE_D1;
    }
    return SF32_DVFS_MODE_D0;
}

static int sf32_rcc_apply_dvfs_for_hclk(uint32_t target_hz)
{
    sf32_dvfs_mode_t mode = sf32_rcc_pick_dvfs_mode(target_hz);
    const sf32_dvfs_cfg_t *cfg = &g_sf32_dvfs_cfg[(uint32_t)mode];
    int32_t vref;

    if (target_hz > SF32_RCC_HCLK_MAX_HZ) {
        return SF32_RCC_ERR_INVALID_TARGET;
    }

    if (mode >= SF32_DVFS_MODE_S0) {
        MODIFY_REG(PMUC->BUCK_VOUT, PMUC_BUCK_VOUT_VOUT_Msk,
                   ((uint32_t)cfg->buck << PMUC_BUCK_VOUT_VOUT_Pos) &
                       PMUC_BUCK_VOUT_VOUT_Msk);
        MODIFY_REG(PMUC->HPSYS_VOUT, PMUC_HPSYS_VOUT_VOUT_Msk,
                   ((uint32_t)cfg->ldo << PMUC_HPSYS_VOUT_VOUT_Pos) &
                       PMUC_HPSYS_VOUT_VOUT_Msk);
        CLEAR_BIT(HPSYS_CFG->SYSCR, HPSYS_CFG_SYSCR_LDO_VSEL_Msk);
        HPSYS_CFG->ULPMCR = cfg->ulpmcr;
        sf32_rcc_delay_cycles(20000UL);
    } else {
        vref = (int32_t)g_sf32_dvfs_cfg[SF32_DVFS_MODE_S0].ldo +
               (int32_t)cfg->ldo_offset;
        if (vref < 0) {
            vref = 0;
        }
        MODIFY_REG(PMUC->BUCK_CR2, PMUC_BUCK_CR2_SET_VOUT_M_Msk,
                   ((uint32_t)cfg->buck << PMUC_BUCK_CR2_SET_VOUT_M_Pos) &
                       PMUC_BUCK_CR2_SET_VOUT_M_Msk);
        MODIFY_REG(PMUC->HPSYS_LDO, PMUC_HPSYS_LDO_VREF_Msk,
                   ((uint32_t)vref << PMUC_HPSYS_LDO_VREF_Pos) &
                       PMUC_HPSYS_LDO_VREF_Msk);
        HPSYS_CFG->ULPMCR = cfg->ulpmcr;
        SET_BIT(HPSYS_CFG->SYSCR, HPSYS_CFG_SYSCR_LDO_VSEL_Msk);
    }

    return SF32_RCC_OK;
}

static void sf32_rcc_systick_reinit(uint32_t hclk_hz)
{
    const uint32_t syst_csr_enable = (1UL << 0);
    const uint32_t syst_csr_tickint = (1UL << 1);
    const uint32_t syst_csr_clksource = (1UL << 2);

    if (hclk_hz == 0U) {
        hclk_hz = SF32_RCC_HCLK_HXT48_HZ;
    }

    SYST_CSR = 0U;
    SYST_RVR = (hclk_hz / 1000UL) - 1UL;
    SYST_CVR = 0U;
    SYST_CSR = syst_csr_clksource | syst_csr_tickint | syst_csr_enable;
}

__attribute__((weak)) void SysTick_Handler(void)
{
    ++uwTick;
}

static uint32_t sf32_rcc_get_sysclk_hz(void)
{
    uint32_t sel_sys = (HPSYS_RCC->CSR.R & HPSYS_RCC_CSR_SEL_SYS_Msk) >>
                       HPSYS_RCC_CSR_SEL_SYS_Pos;

    switch (sel_sys) {
    case 3U: {
        uint32_t dll1cr = HPSYS_RCC->DLL1CR.R;
        uint32_t stg;

        if ((dll1cr & HPSYS_RCC_DLL1CR_EN) == 0U) {
            return 0U;
        }

        stg = (dll1cr & HPSYS_RCC_DLL1CR_STG_Msk) >> HPSYS_RCC_DLL1CR_STG_Pos;
        if ((dll1cr & HPSYS_RCC_DLL1CR_OUT_DIV2_EN) != 0U) {
            return ((stg + 1U) * 24000000UL) / 2UL;
        }
        return (stg + 1U) * 24000000UL;
    }
    case 1U:
    case 0U:
    default:
        return SF32_RCC_HCLK_HXT48_HZ;
    }
}

uint32_t rcc_get_system_hz(void)
{
    uint32_t sysclk = sf32_rcc_get_sysclk_hz();
    uint32_t hdiv = (HPSYS_RCC->CFGR.R & HPSYS_RCC_CFGR_HDIV_Msk) >>
                    HPSYS_RCC_CFGR_HDIV_Pos;

    if (sysclk == 0U) {
        return 0U;
    }
    if (hdiv == 0U) {
        hdiv = 1U;
    }

    return sysclk / hdiv;
}

static int rcc_set_system_hz_impl(uint32_t target_hz)
{
    uint32_t stg;
    uint32_t value;
    uint32_t timeout;
    uint32_t hclk_hz;

    if (target_hz == SF32_RCC_HCLK_HXT48_HZ) {
        MODIFY_REG(HPSYS_RCC->CSR.R, HPSYS_RCC_CSR_SEL_SYS_Msk,
                   (1UL << HPSYS_RCC_CSR_SEL_SYS_Pos));
        MODIFY_REG(HPSYS_RCC->CFGR.R, HPSYS_RCC_CFGR_HDIV_Msk,
                   (1UL << HPSYS_RCC_CFGR_HDIV_Pos));
        sf32_rcc_systick_reinit(rcc_get_system_hz());
        return SF32_RCC_OK;
    }

    if ((target_hz < 72000000UL) || (target_hz > SF32_RCC_HCLK_MAX_HZ) ||
        ((target_hz % 24000000UL) != 0U)) {
        return SF32_RCC_ERR_INVALID_TARGET;
    }

    if (sf32_rcc_apply_dvfs_for_hclk(target_hz) != SF32_RCC_OK) {
        return SF32_RCC_ERR_DVFS;
    }

    stg = (target_hz / 24000000UL) - 1UL;

    SET_BIT(PMUC->HXT_CR1, PMUC_HXT_CR1_EN | PMUC_HXT_CR1_BUF_EN |
                               PMUC_HXT_CR1_BUF_DIG_EN |
                               PMUC_HXT_CR1_BUF_DLL_EN | PMUC_HXT_CR1_GM_EN);

    MODIFY_REG(HPSYS_RCC->CSR.R, HPSYS_RCC_CSR_SEL_SYS_Msk,
               (1UL << HPSYS_RCC_CSR_SEL_SYS_Pos));

    SET_BIT(HPSYS_CFG->CAU2_CR,
            HPSYS_CFG_CAU2_CR_HPBG_EN_Msk |
                HPSYS_CFG_CAU2_CR_HPBG_VDDPSW_EN_Msk);

    value = HPSYS_RCC->DLL1CR.R;
    value &= ~(HPSYS_RCC_DLL1CR_EN | HPSYS_RCC_DLL1CR_STG_Msk |
               HPSYS_RCC_DLL1CR_OUT_DIV2_EN);
    value |= HPSYS_RCC_DLL1CR_XTALIN_EN | HPSYS_RCC_DLL1CR_IN_DIV2_EN |
             (stg << HPSYS_RCC_DLL1CR_STG_Pos);
    HPSYS_RCC->DLL1CR.R = value;
    HPSYS_RCC->DLL1CR.R = value | HPSYS_RCC_DLL1CR_EN;

    timeout = 1000000UL;
    while (((HPSYS_RCC->DLL1CR.R & HPSYS_RCC_DLL1CR_READY) == 0U) &&
           (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        return SF32_RCC_ERR_DLL1_LOCK_TIMEOUT;
    }

    MODIFY_REG(HPSYS_RCC->CFGR.R,
               HPSYS_RCC_CFGR_HDIV_Msk | HPSYS_RCC_CFGR_PDIV1_Msk |
                   HPSYS_RCC_CFGR_PDIV2_Msk,
               (1UL << HPSYS_RCC_CFGR_HDIV_Pos) |
                   (1UL << HPSYS_RCC_CFGR_PDIV1_Pos) |
                   (6UL << HPSYS_RCC_CFGR_PDIV2_Pos));
    MODIFY_REG(HPSYS_RCC->CSR.R, HPSYS_RCC_CSR_SEL_SYS_Msk,
               (3UL << HPSYS_RCC_CSR_SEL_SYS_Pos));

    hclk_hz = rcc_get_system_hz();
    sf32_rcc_systick_reinit(hclk_hz);

    if (hclk_hz == target_hz) {
        return SF32_RCC_OK;
    }

    return SF32_RCC_ERR_SWITCH_VERIFY;
}

void rcc_init(void)
{
    uwTick = 0U;
    (void)rcc_set_system_hz_impl(SF32_RCC_HCLK_HXT48_HZ);
}

void rcc_set_system_hz(uint32_t hz)
{
    (void)rcc_set_system_hz_impl(hz);
}
