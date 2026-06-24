#include "ll_gpio.h"

#include "SF32LB52.h"

typedef struct {
    volatile uint32_t *dir;
    volatile uint32_t *dor;
    volatile uint32_t *dosr;
    volatile uint32_t *docr;
    volatile uint32_t *doer;
    volatile uint32_t *doesr;
    volatile uint32_t *doecr;
    uint32_t mask;
} sf32lb52_gpio_bank_t;

static sf32lb52_gpio_bank_t gpio_bank_select(uint32_t pin)
{
    HPSYS_GPIO_TypeDef *gpio = HPSYS_GPIO;

    if (pin < 32U) {
        return (sf32lb52_gpio_bank_t){
            &gpio->DIR0.R,
            &gpio->DOR0.R,
            &gpio->DOSR0.R,
            &gpio->DOCR0.R,
            &gpio->DOER0.R,
            &gpio->DOESR0.R,
            &gpio->DOECR0.R,
            1UL << pin,
        };
    }

    return (sf32lb52_gpio_bank_t){
        &gpio->DIR1.R,
        &gpio->DOR1.R,
        &gpio->DOSR1.R,
        &gpio->DOCR1.R,
        &gpio->DOER1.R,
        &gpio->DOESR1.R,
        &gpio->DOECR1.R,
        1UL << (pin - 32U),
    };
}

void sf32lb52_gpio_enable_clock(void)
{
    const uint32_t gpio1_mask = (1UL << 0);

    HPSYS_RCC->ENR2.R |= gpio1_mask;
    HPSYS_RCC->ESR2.R |= gpio1_mask;
}

void sf32lb52_gpio_enable_output(uint32_t pin)
{
    sf32lb52_gpio_bank_t bank = gpio_bank_select(pin);

    sf32lb52_gpio_enable_clock();
    *bank.doesr = bank.mask;
}

void sf32lb52_gpio_disable_output(uint32_t pin)
{
    sf32lb52_gpio_bank_t bank = gpio_bank_select(pin);

    sf32lb52_gpio_enable_clock();
    *bank.doecr = bank.mask;
}

void sf32lb52_gpio_write(uint32_t pin, uint8_t value)
{
    sf32lb52_gpio_bank_t bank = gpio_bank_select(pin);

    if (value == 0U) {
        *bank.docr = bank.mask;
    } else {
        *bank.dosr = bank.mask;
    }
}

uint8_t sf32lb52_gpio_read(uint32_t pin)
{
    sf32lb52_gpio_bank_t bank = gpio_bank_select(pin);
    uint32_t value;

    if ((*bank.doer & bank.mask) != 0U) {
        value = *bank.dor;
    } else {
        value = *bank.dir;
    }

    return ((value & bank.mask) != 0U) ? 1U : 0U;
}

void sf32lb52_gpio_toggle(uint32_t pin)
{
    sf32lb52_gpio_bank_t bank = gpio_bank_select(pin);

    if ((*bank.dor & bank.mask) != 0U) {
        *bank.docr = bank.mask;
    } else {
        *bank.dosr = bank.mask;
    }
}
