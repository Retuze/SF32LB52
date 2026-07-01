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
 * Shared flag: ISR → pollEvent() signal
 * ========================================================================== */
volatile int g_tp_irq_fired;
volatile int g_tp_irq_cnt;

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
    ++g_tp_irq_cnt;
    g_tp_irq_fired = 1;
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

static void print_addr(uint8_t addr, void *user)
{
    (void)user;
    printf(" 0x%02X", (unsigned)addr);
}

void tp_ft6146_init(tp_ft6146_t *tp)
{
    /* 1. Configure I2C pins */
    bb_i2c_init(&tp->i2c);

    /* 2. Configure RST & INT pins */
    pinMode(tp->pin_rst, OUTPUT);
    digitalWrite(tp->pin_rst, LOW);   /* hold in reset */

    pinMode(tp->pin_int, INPUT); /* active-low, external pull-up */

    /* 3. Power-on sequence: RST low 5ms → RST high → wait 80ms */
    digitalWrite(tp->pin_rst, LOW);
    delay_ms(5);
    digitalWrite(tp->pin_rst, HIGH);
    delay_ms(80);

    /* 4. I2C bus scan */
    printf("[ft6146] I2C scan: ");
    bb_i2c_scan(&tp->i2c, print_addr, NULL);
    printf("\r\n");

    /* 5. Chip ID verification */
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
    /* Only point-1 registers are parsed below: TD_STATUS (0x02) + P1 X/Y
     * (0x03..0x06), i.e. buf[1..5] of a bulk read starting at 0x01.  Read just
     * these 6 bytes instead of the full 2 + 6*MAX_POINTS block — it roughly
     * halves the bit-bang I2C transfer, which sits on the per-frame input path. */
    uint8_t buf[6] = {0};
    int ret;

    ret = read_regs(tp, FT_REG_BULK_READ, buf, sizeof(buf));
    if (ret != 0) {
        return -1;
    }

    uint8_t touch_num = buf[1] & 0x0FU;

    /* Track touch state across calls — FT6146's event_flag is only valid
     * for the hardware's internal 100 Hz scan; by the time pollEvent()
     * reads (~18 ms later) the flag may have already advanced from DOWN
     * to MOVE.  We synthesize DOWN / UP from touch_num transitions. */
    static uint8_t prev_touch;

    if (touch_num == 0) {
        if (prev_touch) {
            int x = (int)((buf[2] & 0x0FU) << 8) | (int)buf[3];
            int y = (int)((buf[4] & 0x0FU) << 8) | (int)buf[5];
            *out_event = TP_EVENT_UP;
            *out_x = x;          /* raw is already screen-aligned — no mirror */
            *out_y = y;
            prev_touch = 0;
            return 0;
        }
        return -1;  /* no touch and wasn't touching — no event */
    }

    if (touch_num > TP_FT6146_MAX_POINTS) {
        touch_num = TP_FT6146_MAX_POINTS;
    }

    int x = (int)((buf[2] & 0x0FU) << 8) | (int)buf[3];
    int y = (int)((buf[4] & 0x0FU) << 8) | (int)buf[5];

    if (!prev_touch) {
        *out_event = TP_EVENT_DOWN;   /* transition 0→1: synthesize DOWN */
    } else {
        uint8_t event_flag = (buf[2] >> 6) & 0x03U;
        *out_event = (event_flag == TP_EVENT_UP) ? TP_EVENT_UP : TP_EVENT_MOVE;
    }

    *out_x = x;                       /* raw is already screen-aligned — no mirror */
    *out_y = y;
    prev_touch = (touch_num > 0) ? 1 : 0;

    return 0;
}

bool tp_ft6146_touched(tp_ft6146_t *tp)
{
    return digitalRead(tp->pin_int) == LOW;
}

int tp_ft6146_read_reg(tp_ft6146_t *tp, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return read_regs(tp, reg, buf, len);
}
