/**
 * @file ll_gpio.c
 * @brief GPIO driver — basic I/O + interrupt dispatch.
 */

#include "ll_gpio.h"
#include "ll_nvic.h"  /* nvic_set_priority / nvic_enable_irq */
#include "ll_pinmux.h"
#include "SF32LB52.h"
#include <stddef.h>

/* ==========================================================================
 * Internal — bank register map & helpers
 * ========================================================================== */

typedef struct {
    volatile uint32_t *dir, *dor, *dosr, *docr, *doer, *doesr, *doecr;
    volatile uint32_t *ier, *iesr, *iecr;
    volatile uint32_t *itr, *itsr, *itcr;
    volatile uint32_t *iphr, *iphsr, *iphcr;
    volatile uint32_t *iplr, *iplsr, *iplcr;
    volatile uint32_t *isr;
    uint32_t           mask;
} gpio_bank_t;

static gpio_bank_t gpio_bank_resolve(uint32_t pin)
{
    HPSYS_GPIO_TypeDef *g = HPSYS_GPIO;

    if (pin < 32U) {
        return (gpio_bank_t){
            &g->DIR0.R,   &g->DOR0.R,
            &g->DOSR0.R,  &g->DOCR0.R,
            &g->DOER0.R,  &g->DOESR0.R,  &g->DOECR0.R,
            &g->IER0.R,   &g->IESR0.R,   &g->IECR0.R,
            &g->ITR0.R,   &g->ITSR0.R,   &g->ITCR0.R,
            &g->IPHR0.R,  &g->IPHSR0.R,  &g->IPHCR0.R,
            &g->IPLR0.R,  &g->IPLSR0.R,  &g->IPLCR0.R,
            &g->ISR0.R,
            1UL << pin,
        };
    }
    return (gpio_bank_t){
        &g->DIR1.R,   &g->DOR1.R,
        &g->DOSR1.R,  &g->DOCR1.R,
        &g->DOER1.R,  &g->DOESR1.R,  &g->DOECR1.R,
        &g->IER1.R,   &g->IESR1.R,   &g->IECR1.R,
        &g->ITR1.R,   &g->ITSR1.R,   &g->ITCR1.R,
        &g->IPHR1.R,  &g->IPHSR1.R,  &g->IPHCR1.R,
        &g->IPLR1.R,  &g->IPLSR1.R,  &g->IPLCR1.R,
        &g->ISR1.R,
        1UL << (pin - 32U),
    };
}

static void gpio_clk_enable(void)
{
    HPSYS_RCC->ENR2.R |= 1UL;
    HPSYS_RCC->ESR2.R |= 1UL;
}

/* ==========================================================================
 * pinMode helper — pinmux flags for each input mode
 * ========================================================================== */

static uint32_t input_flags(uint8_t mode)
{
    switch (mode) {
    case INPUT_PULLUP:   return PINMUX_PULL_UP   | PINMUX_INPUT_ENABLE;
    case INPUT_PULLDOWN: return PINMUX_PULL_DOWN | PINMUX_INPUT_ENABLE;
    default:             return PINMUX_PULL_NONE | PINMUX_INPUT_ENABLE;
    }
}

/* ==========================================================================
 * Basic I/O
 * ========================================================================== */

void pinMode(uint32_t pin, uint8_t mode)
{
    gpio_clk_enable();

    if (mode == OUTPUT) {
        pinmux_config(pin, 0U,
                                   PINMUX_PULL_NONE | PINMUX_DRIVE_3);
        gpio_bank_t b = gpio_bank_resolve(pin);
        *b.doesr = b.mask;
        return;
    }

    pinmux_config(pin, 0U, input_flags(mode));
    gpio_bank_t b = gpio_bank_resolve(pin);
    *b.doecr = b.mask;
}

void digitalWrite(uint32_t pin, uint8_t value)
{
    gpio_bank_t b = gpio_bank_resolve(pin);
    if (value == 0U) { *b.docr = b.mask; }
    else             { *b.dosr = b.mask; }
}

uint8_t digitalRead(uint32_t pin)
{
    gpio_bank_t b = gpio_bank_resolve(pin);
    uint32_t val = (*b.doer & b.mask) ? *b.dor : *b.dir;
    return (val & b.mask) ? HIGH : LOW;
}

void digitalToggle(uint32_t pin)
{
    gpio_bank_t b = gpio_bank_resolve(pin);
    if (*b.dor & b.mask) { *b.docr = b.mask; }
    else                 { *b.dosr = b.mask; }
}

/* ==========================================================================
 * Interrupt — attachInterrupt / detachInterrupt + central ISR
 * ========================================================================== */

#define IRQ_NVIC_PRIO  ((2U << 6U) | 1U)
#define IRQ_MAX_PIN     64U

static gpio_irq_cb_t  irq_cb[IRQ_MAX_PIN];
static void          *irq_arg[IRQ_MAX_PIN];

static void irq_disable(uint32_t pin)
{
    gpio_bank_t b = gpio_bank_resolve(pin);
    *b.iecr = b.mask;
}

void attachInterrupt(uint32_t pin, gpio_irq_cb_t cb, uint32_t mode, void *arg)
{
    if (pin >= IRQ_MAX_PIN) return;

    gpio_bank_t b = gpio_bank_resolve(pin);

    irq_cb[pin]  = cb;
    irq_arg[pin] = arg;

    /* Configure edge trigger + polarity */
    *b.itsr = b.mask;                  /* edge (not level) */

    switch (mode) {
    case RISING:
        *b.iphsr = b.mask;             /* rising enable  */
        *b.iplcr = b.mask;             /* falling disable */
        break;
    case FALLING:
        *b.iphcr = b.mask;             /* rising disable  */
        *b.iplsr = b.mask;             /* falling enable  */
        break;
    default: /* CHANGE */
        *b.iphsr = b.mask;             /* both edges */
        *b.iplsr = b.mask;
        break;
    }

    *b.isr  = b.mask;                  /* clear stale pending */
    *b.iesr = b.mask;                  /* enable */

    nvic_set_priority(GPIO1_IRQn, IRQ_NVIC_PRIO);
    nvic_enable_irq(GPIO1_IRQn);
}

void detachInterrupt(uint32_t pin)
{
    if (pin >= IRQ_MAX_PIN) return;
    irq_disable(pin);
    irq_cb[pin]  = NULL;
    irq_arg[pin] = NULL;
}

/* ── Central ISR ────────────────────────────────────────────────────────── */

static void dispatch_bank(volatile uint32_t *isr, uint32_t base_pin)
{
    uint32_t pending = *isr;
    while (pending) {
        uint32_t bit = pending & -pending;
        uint32_t pin = base_pin + (uint32_t)__builtin_ctz(pending);
        *isr = bit;                                      /* write-1-clear */
        if (irq_cb[pin]) irq_cb[pin](pin, irq_arg[pin]);
        pending ^= bit;
    }
}

void GPIO1_IRQHandler(void)
{
    dispatch_bank(&HPSYS_GPIO->ISR0.R, 0U);    /* pins  0–31 */
    dispatch_bank(&HPSYS_GPIO->ISR1.R, 32U);   /* pins 32–63 */
}
