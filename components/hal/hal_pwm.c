#include "hal_pwm.h"

#include "ll_atim.h"

#define PWM_DEFAULT_FREQUENCY_HZ 1000U
#define PWM_DEFAULT_RESOLUTION   8U
#define PWM_MAX_RESOLUTION       16U

typedef struct {
    uint32_t pin;
    uint8_t channel;
    uint8_t attached;
} pwm_pin_binding_t;

static uint32_t s_frequency_hz = PWM_DEFAULT_FREQUENCY_HZ;
static uint8_t s_resolution_bits = PWM_DEFAULT_RESOLUTION;
static pwm_pin_binding_t s_bindings[4] = {
    {36U, ATIM_CH1, 0U},
    {37U, ATIM_CH2, 0U},
    {38U, ATIM_CH3, 0U},
    {39U, ATIM_CH4, 0U},
};

static pwm_pin_binding_t *pwm_find_binding(uint32_t pin)
{
    uint32_t i;

    for (i = 0U; i < (sizeof(s_bindings) / sizeof(s_bindings[0])); i++) {
        if (s_bindings[i].pin == pin) {
            return &s_bindings[i];
        }
    }

    return 0;
}

static pwm_pin_binding_t *pwm_find_channel_binding(uint8_t channel)
{
    uint32_t i;

    for (i = 0U; i < (sizeof(s_bindings) / sizeof(s_bindings[0])); i++) {
        if (s_bindings[i].channel == channel) {
            return &s_bindings[i];
        }
    }

    return 0;
}

void analogWriteFrequency(uint32_t frequency_hz)
{
    if (frequency_hz != 0U) {
        s_frequency_hz = frequency_hz;
    }
}

void analogWriteResolution(uint8_t bits)
{
    if (bits == 0U) {
        bits = 1U;
    } else if (bits > PWM_MAX_RESOLUTION) {
        bits = PWM_MAX_RESOLUTION;
    }

    s_resolution_bits = bits;
}

int pwmAttachPin(uint32_t pin, uint8_t channel)
{
    pwm_pin_binding_t *binding;

    if ((channel < ATIM_CH1) || (channel > ATIM_CH4)) {
        return -1;
    }

    binding = pwm_find_binding(pin);
    if (binding == 0) {
        binding = pwm_find_channel_binding(channel);
    }
    if (binding == 0) {
        return -2;
    }

    binding->pin = pin;
    binding->channel = channel;
    binding->attached = 0U;

    return 0;
}

int analogWrite(uint32_t pin, uint32_t value)
{
    pwm_pin_binding_t *binding = pwm_find_binding(pin);
    uint32_t max_value;
    uint32_t period_ticks;
    uint32_t pulse_ticks;
    int ret;

    if (binding == 0) {
        return -2;
    }

    if (binding->attached == 0U) {
        ret = atim_pwm_pin(pin, binding->channel);
        if (ret != 0) {
            return ret;
        }
        binding->attached = 1U;
    }

    ret = atim_pwm_init(binding->channel, s_frequency_hz);
    if (ret != 0) {
        return ret;
    }

    max_value = (1UL << s_resolution_bits) - 1UL;
    if (value > max_value) {
        value = max_value;
    }

    period_ticks = atim_pwm_period();
    pulse_ticks = (uint32_t)(((uint64_t)value * period_ticks) / max_value);

    return atim_pwm_write(binding->channel, pulse_ticks);
}
