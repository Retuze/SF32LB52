/**
 * @file tp_ft6146.c
 * @brief FT6146-M00 capacitive touch panel driver.
 *
 * Uses bit-bang I2C (bb_i2c) for register access.  CTP_INT is registered via
 * gpio_irq_attach() — the central GPIO1_IRQHandler dispatches to our callback.
 * The callback sets a flag and masks the interrupt; pollEvent() reads the touch
 * data then re-enables.
 */

#include "tp_ft6146.h"
#include "hal.h"
#include <stdio.h>

/* ==========================================================================
 * FT6146 register map
 * ========================================================================== */
#define FT_DEV_ADDR      0x38U   /* 7-bit I2C address */
#define FT_REG_TD_STATUS 0x02U   /* Touch-down status: lower nibble = point count */
#define FT_REG_P1_XH     0x03U   /* Point 1 X high byte */
#define FT_REG_P1_XL     0x04U   /* Point 1 X low byte */
#define FT_REG_P1_YH     0x05U   /* Point 1 Y high byte */
#define FT_REG_P1_YL     0x06U   /* Point 1 Y low byte */
#define FT_REG_READ_ID_H 0xA3U   /* Chip ID high (expected: 0x64 = 'd') */
#define FT_REG_READ_ID_L 0x9FU   /* Chip ID low  (expected: 0x56 = 'V') */

/* Multi-point bulk read start address (2 + 6 * N bytes) */
#define FT_REG_BULK_READ 0x01U

/* ==========================================================================
 * Touch events (matches FT6146 hardware encoding)
 * ========================================================================== */
enum {
    TP_EVENT_DOWN = 0,
    TP_EVENT_UP   = 1,
    TP_EVENT_MOVE = 2,
    TP_EVENT_RESERVE = 3,
};

/* ==========================================================================
 * Shared flag: ISR → pollEvent() signal
 * ========================================================================== */
volatile int g_tp_irq_fired;

/* ==========================================================================
 * I2C helpers
 * ========================================================================== */

__attribute__((unused))
static int write_reg(const tp_ft6146_t *tp, uint8_t reg, uint8_t data)
{
    return bb_i2c_mem_write(&tp->i2c, FT_DEV_ADDR, reg, &data, 1U);
}

static int read_regs(const tp_ft6146_t *tp, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return bb_i2c_mem_read(&tp->i2c, FT_DEV_ADDR, reg, buf, len);
}

/* ==========================================================================
 * Delay (blocking, for power-on sequence)
 * ========================================================================== */

static void delay_ms(uint32_t ms)
{
    /* Rough busy-wait at ~240 MHz. Each iteration ~3 cycles → ~80M iter/sec. */
    for (volatile uint32_t i = 0U; i < ms * 80000UL; ++i) {
        __asm volatile ("" ::: "memory");
    }
}

/* ==========================================================================
 * Interrupt callback — dispatched by central GPIO1_IRQHandler
 * ========================================================================== */

static void ft6146_irq_cb(uint32_t pin, void *arg)
{
    (void)pin;
    (void)arg;
    g_tp_irq_fired = 1;
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void tp_ft6146_init(tp_ft6146_t *tp)
{
    /* 1. Configure I2C pins */
    bb_i2c_init(&tp->i2c);

    /* 2. Configure RST & INT pins */
    pinMode(tp->pin_rst, OUTPUT);
    digitalWrite(tp->pin_rst, LOW);   /* hold in reset */

    pinMode(tp->pin_int, INPUT_PULLUP); /* active-low, pull-up when idle */

    /* 3. Power-on sequence: RST low 5ms → RST high → wait 80ms */
    digitalWrite(tp->pin_rst, LOW);
    delay_ms(5);
    digitalWrite(tp->pin_rst, HIGH);
    delay_ms(80);

    /* 4. Chip ID verification (non-fatal: warn only) */
    uint8_t id_h = 0, id_l = 0;
    if (read_regs(tp, FT_REG_READ_ID_H, &id_h, 1U) == 0 &&
        read_regs(tp, FT_REG_READ_ID_L, &id_l, 1U) == 0) {
        printf("[ft6146] ID: 0x%02X 0x%02X %s\r\n",
               id_h, id_l,
               (id_h == 0x64U && id_l == 0x56U) ? "OK" : "UNKNOWN");
    } else {
        printf("[ft6146] ID read failed — check I2C wiring\r\n");
    }

    /* 5. Register falling-edge interrupt */
    attachInterrupt(tp->pin_int, ft6146_irq_cb, FALLING, NULL);

    printf("[ft6146] init done, INT pin=%lu\r\n", (unsigned long)tp->pin_int);
}

int tp_ft6146_read(tp_ft6146_t *tp, int *out_x, int *out_y, int *out_event)
{
    uint8_t buf[2U + 6U * TP_FT6146_MAX_POINTS] = {0};
    int ret;

    ret = read_regs(tp, FT_REG_BULK_READ, buf, sizeof(buf));
    if (ret != 0) {
        return -1;
    }

    uint8_t touch_num = buf[1] & 0x0FU;

    if (touch_num == 0) {
        /* No finger — report UP at last known position */
        int x = (int)((buf[2] & 0x0FU) << 8) | (int)buf[3];
        int y = (int)((buf[4] & 0x0FU) << 8) | (int)buf[5];

        *out_event = TP_EVENT_UP;
        *out_x = (int)tp->max_x - x;
        *out_y = (int)tp->max_y - y;
        return 0;
    }

    if (touch_num > TP_FT6146_MAX_POINTS) {
        touch_num = TP_FT6146_MAX_POINTS;
    }

    /* Report first touch point */
    uint8_t event_flag = (buf[2] >> 6) & 0x03U;
    int x = (int)((buf[2] & 0x0FU) << 8) | (int)buf[3];
    int y = (int)((buf[4] & 0x0FU) << 8) | (int)buf[5];

    *out_event = (event_flag == TP_EVENT_UP) ? TP_EVENT_UP : TP_EVENT_DOWN;
    *out_x = tp->max_x - x;
    *out_y = tp->max_y - y;

    return 0;
}

bool tp_ft6146_touched(tp_ft6146_t *tp)
{
    return digitalRead(tp->pin_int) == LOW;
}
