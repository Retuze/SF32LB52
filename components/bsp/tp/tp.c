/**
 * @file tp.c
 * @brief Touch panel framework — panel singleton, delegates to bus + IC driver.
 */

#include "tp.h"
#include "board.h"
#include "hal.h"
#include <stdio.h>

/* ── Panel state ─────────────────────────────────────────────────────── */

static struct {
    const tp_bus_t *bus;
    const tp_ic_t  *ic;
    const void     *bus_config;  /* e.g., bb_i2c_t* for I2C bus */
    uint32_t pin_rst, pin_irq;
    uint8_t  dev_addr;
} g = {
    .pin_rst = 0xFFFFFFFF, .pin_irq = 0xFFFFFFFF
};

/* Expose bus_config for bus drivers (read-only) */
const void *_tp_bus_config_ptr = NULL;

/* ── IRQ signaling ────────────────────────────────────────────────────── */

volatile int g_tp_irq_fired;
volatile int g_tp_irq_cnt;

/* ── Delay (blocking, for power-on sequence) ──────────────────────────── */

static void delay_ms(uint32_t ms)
{
    /* Rough busy-wait at ~240 MHz. Each iteration ~3 cycles → ~80M iter/sec. */
    for (volatile uint32_t i = 0U; i < ms * 80000UL; ++i) {
        __asm volatile ("" ::: "memory");
    }
}

/* ── Interrupt callback ───────────────────────────────────────────────── */

static void tp_irq_cb(uint32_t pin, void *arg)
{
    (void)pin;
    (void)arg;
    ++g_tp_irq_cnt;
    g_tp_irq_fired = 1;
}

/* ── Registration ─────────────────────────────────────────────────────── */

void tp_set_bus(const tp_bus_t *b, const void *cfg) {
    g.bus = b;
    g.bus_config = cfg;
    _tp_bus_config_ptr = cfg;  /* sync for bus drivers */
}
void tp_set_ic(const tp_ic_t *i)                     { g.ic = i; g.dev_addr = i->dev_addr; }
void tp_set_ctrl_pins(uint32_t rst, uint32_t irq)   { g.pin_rst = rst; g.pin_irq = irq; }

/* ── Init ─────────────────────────────────────────────────────────────── */

int tp_init(void)
{
    if (!g.bus || !g.ic) return -1;

    /* 1. Initialize bus (e.g., configure I2C pins) */
    g.bus->init(g.bus_config);

    /* 2. Configure RST & IRQ pins */
    if (g.pin_rst != 0xFFFFFFFF) {
        pinMode(g.pin_rst, OUTPUT);
        digitalWrite(g.pin_rst, LOW);   /* hold in reset */
    }

    if (g.pin_irq != 0xFFFFFFFF) {
        pinMode(g.pin_irq, INPUT);      /* active-low, external pull-up */
    }

    /* 3. Power-on sequence: RST low 5ms → RST high → wait 80ms */
    if (g.pin_rst != 0xFFFFFFFF) {
        digitalWrite(g.pin_rst, LOW);
        delay_ms(5);
        digitalWrite(g.pin_rst, HIGH);
        delay_ms(80);
    }

    /* 4. Initialize IC (chip-specific init + ID check) */
    int ret = g.ic->init(g.bus, g.dev_addr);
    if (ret != 0) {
        printf("[tp] IC init failed: %d\r\n", ret);
        return ret;
    }

    /* 5. Register falling-edge interrupt */
    if (g.pin_irq != 0xFFFFFFFF) {
        attachInterrupt(g.pin_irq, tp_irq_cb, FALLING, NULL);
    }

    printf("[tp] init done (%s)\r\n", g.ic->name);
    return 0;
}

/* ── Public API ───────────────────────────────────────────────────────── */

int tp_read(int *x, int *y, int *event)
{
    if (!g.ic || !g.bus) return -1;
    return g.ic->read_touch(g.bus, g.dev_addr, x, y, event);
}

bool tp_touched(void)
{
    if (g.pin_irq == 0xFFFFFFFF) return false;
    return digitalRead(g.pin_irq) == LOW;
}

uint32_t tp_read_id(void)
{
    if (!g.ic || !g.bus) return 0;
    return g.ic->read_id(g.bus, g.dev_addr);
}
