#include "ll_atim.h"

#include "SF32LB52.h"
#include "ll_pinmux.h"
#include "ll_rcc.h"

#define ATIM_PWM_MODE_1       6UL
#define ATIM_MAX_PERIOD_TICKS 0xFFFFFFFFUL
#define ATIM_PINMUX_FSEL_TIM  5U

static uint32_t s_period_ticks;

static int atim_valid_channel(uint8_t channel)
{
    return (channel >= SF32_ATIM_CHANNEL_1) && (channel <= SF32_ATIM_CHANNEL_4);
}

static volatile uint32_t *atim_ccr(uint8_t channel)
{
    switch (channel) {
    case SF32_ATIM_CHANNEL_1:
        return &ATIM1->CCR1;
    case SF32_ATIM_CHANNEL_2:
        return &ATIM1->CCR2;
    case SF32_ATIM_CHANNEL_3:
        return &ATIM1->CCR3;
    case SF32_ATIM_CHANNEL_4:
    default:
        return &ATIM1->CCR4;
    }
}

static uint32_t atim_channel_enable_bit(uint8_t channel)
{
    switch (channel) {
    case SF32_ATIM_CHANNEL_1:
        return ATIM_CCER_CC1E;
    case SF32_ATIM_CHANNEL_2:
        return ATIM_CCER_CC2E;
    case SF32_ATIM_CHANNEL_3:
        return ATIM_CCER_CC3E;
    case SF32_ATIM_CHANNEL_4:
    default:
        return ATIM_CCER_CC4E;
    }
}

static void atim_enable_clocks(void)
{
    sf32lb52_pinmux_enable_clock();
    HPSYS_RCC->ENR2.R |= HPSYS_RCC_ENR2_ATIM1_Msk;
    HPSYS_RCC->ESR2.R |= HPSYS_RCC_ESR2_ATIM1_Msk;
}

static void atim_configure_pwm_channel(uint8_t channel)
{
    switch (channel) {
    case SF32_ATIM_CHANNEL_1:
        MODIFY_REG(ATIM1->CCMR1,
                   ATIM_CCMR1_CC1S_Msk | ATIM_CCMR1_OC1M_Msk,
                   ATIM_CCMR1_OC1PE |
                       (ATIM_PWM_MODE_1 << ATIM_CCMR1_OC1M_Pos));
        break;
    case SF32_ATIM_CHANNEL_2:
        MODIFY_REG(ATIM1->CCMR1,
                   ATIM_CCMR1_CC2S_Msk | ATIM_CCMR1_OC2M_Msk,
                   ATIM_CCMR1_OC2PE |
                       (ATIM_PWM_MODE_1 << ATIM_CCMR1_OC2M_Pos));
        break;
    case SF32_ATIM_CHANNEL_3:
        MODIFY_REG(ATIM1->CCMR2,
                   ATIM_CCMR2_CC3S_Msk | ATIM_CCMR2_OC3M_Msk,
                   ATIM_CCMR2_OC3PE |
                       (ATIM_PWM_MODE_1 << ATIM_CCMR2_OC3M_Pos));
        break;
    case SF32_ATIM_CHANNEL_4:
    default:
        MODIFY_REG(ATIM1->CCMR2,
                   ATIM_CCMR2_CC4S_Msk | ATIM_CCMR2_OC4M_Msk,
                   ATIM_CCMR2_OC4PE |
                       (ATIM_PWM_MODE_1 << ATIM_CCMR2_OC4M_Pos));
        break;
    }
}

int sf32lb52_atim_pwm_configure(uint8_t channel, uint32_t frequency_hz)
{
    uint32_t timer_hz;
    uint32_t prescaler;
    uint64_t ticks;

    if (!atim_valid_channel(channel)) {
        return SF32_ATIM_ERR_BAD_CHANNEL;
    }
    if (frequency_hz == 0U) {
        return SF32_ATIM_ERR_BAD_ARGUMENT;
    }

    atim_enable_clocks();

    timer_hz = rcc_get_system_hz();
    if (timer_hz == 0U) {
        return SF32_ATIM_ERR_CLOCK;
    }

    ticks = timer_hz / frequency_hz;
    if (ticks < 2U) {
        ticks = 2U;
    }

    prescaler = (uint32_t)((ticks + ATIM_MAX_PERIOD_TICKS - 1ULL) /
                           ATIM_MAX_PERIOD_TICKS);
    if (prescaler == 0U) {
        prescaler = 1U;
    }

    ticks /= prescaler;
    if (ticks < 2U) {
        ticks = 2U;
    }

    s_period_ticks = (uint32_t)ticks;

    ATIM1->CR1 &= ~ATIM_CR1_CEN;
    ATIM1->PSC = prescaler - 1U;
    ATIM1->ARR = s_period_ticks - 1U;
    ATIM1->CR1 |= ATIM_CR1_ARPE;

    atim_configure_pwm_channel(channel);
    ATIM1->CCER |= atim_channel_enable_bit(channel);
    ATIM1->BDTR |= ATIM_BDTR_MOE;
    ATIM1->EGR = ATIM_EGR_UG;
    ATIM1->CR1 |= ATIM_CR1_CEN;

    return SF32_ATIM_OK;
}

int sf32lb52_atim_pwm_attach_pin(uint32_t pin, uint8_t channel)
{
    static const uint32_t pos[] = {
        HPSYS_CFG_ATIM1_PINR1_CH1_PIN_Pos,
        HPSYS_CFG_ATIM1_PINR1_CH2_PIN_Pos,
        HPSYS_CFG_ATIM1_PINR1_CH3_PIN_Pos,
        HPSYS_CFG_ATIM1_PINR1_CH4_PIN_Pos,
    };
    static const uint32_t mask[] = {
        HPSYS_CFG_ATIM1_PINR1_CH1_PIN_Msk,
        HPSYS_CFG_ATIM1_PINR1_CH2_PIN_Msk,
        HPSYS_CFG_ATIM1_PINR1_CH3_PIN_Msk,
        HPSYS_CFG_ATIM1_PINR1_CH4_PIN_Msk,
    };
    uint32_t index;

    if (!atim_valid_channel(channel)) {
        return SF32_ATIM_ERR_BAD_CHANNEL;
    }
    if (pin >= HPSYS_PINMUX_PAD_COUNT) {
        return SF32_ATIM_ERR_BAD_ARGUMENT;
    }

    atim_enable_clocks();

    index = (uint32_t)channel - 1U;
    MODIFY_REG(HPSYS_CFG->ATIM1_PINR1, mask[index],
               (pin << pos[index]) & mask[index]);
    sf32lb52_pinmux_config_pad(pin, ATIM_PINMUX_FSEL_TIM,
                               SF32_PINMUX_PULL_NONE | SF32_PINMUX_DRIVE_3);

    return SF32_ATIM_OK;
}

int sf32lb52_atim_pwm_write_raw(uint8_t channel, uint32_t pulse_ticks)
{
    if (!atim_valid_channel(channel)) {
        return SF32_ATIM_ERR_BAD_CHANNEL;
    }

    if (s_period_ticks == 0U) {
        return SF32_ATIM_ERR_BAD_ARGUMENT;
    }
    if (pulse_ticks > s_period_ticks) {
        pulse_ticks = s_period_ticks;
    }

    *atim_ccr(channel) = (pulse_ticks == 0U) ? 0U : (pulse_ticks - 1U);
    ATIM1->EGR = ATIM_EGR_UG;

    return SF32_ATIM_OK;
}

uint32_t sf32lb52_atim_pwm_get_period_ticks(void)
{
    return s_period_ticks;
}
