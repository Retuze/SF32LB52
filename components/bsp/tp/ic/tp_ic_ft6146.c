/**
 * @file tp_ic_ft6146.c
 * @brief FocalTech FT6146-M00 touch IC driver.
 *
 * Chip-specific initialization and touch reading logic.
 */

#include "tp.h"
#include "bb_i2c.h"
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
#define FT_REG_BULK_READ 0x01U   /* Multi-point bulk read start address */

#define FT6146_MAX_POINTS 2

/* ==========================================================================
 * I2C bus scan helper
 * ========================================================================== */

static void print_addr(uint8_t addr, void *user)
{
    (void)user;
    printf(" 0x%02X", (unsigned)addr);
}

/* ==========================================================================
 * IC driver implementation
 * ========================================================================== */

static int ft6146_init(const tp_bus_t *bus, uint8_t dev_addr)
{
    /* 1. I2C bus scan */
    printf("[ft6146] I2C scan: ");
    extern const void *_tp_bus_config_ptr;
    const bb_i2c_t *i2c = (const bb_i2c_t *)_tp_bus_config_ptr;
    bb_i2c_scan(i2c, print_addr, NULL);
    printf("\r\n");

    /* 2. Chip ID verification */
    uint8_t id_h = 0, id_l = 0;
    if (bus->read(dev_addr, FT_REG_READ_ID_H, &id_h, 1U) == 0 &&
        bus->read(dev_addr, FT_REG_READ_ID_L, &id_l, 1U) == 0) {
        printf("[ft6146] ID: 0x%02X 0x%02X %s\r\n",
               id_h, id_l,
               (id_h == 0x64U && id_l == 0x56U) ? "OK" : "UNKNOWN");
        if (id_h != 0x64U || id_l != 0x56U) {
            return -1;
        }
    } else {
        printf("[ft6146] ID read failed — check I2C wiring\r\n");
        return -1;
    }

    return 0;
}

static int ft6146_read_touch(const tp_bus_t *bus, uint8_t dev_addr,
                              int *out_x, int *out_y, int *out_event)
{
    /* Read TD_STATUS (0x02) + P1 X/Y (0x03..0x06) — 6 bytes total starting at 0x01 */
    uint8_t buf[6] = {0};
    int ret = bus->read(dev_addr, FT_REG_BULK_READ, buf, sizeof(buf));
    if (ret != 0) {
        return -1;
    }

    uint8_t touch_num = buf[1] & 0x0FU;

    /* Track touch state across calls — synthesize DOWN/UP from touch_num transitions */
    static uint8_t prev_touch = 0;

    if (touch_num == 0) {
        if (prev_touch) {
            int x = (int)((buf[2] & 0x0FU) << 8) | (int)buf[3];
            int y = (int)((buf[4] & 0x0FU) << 8) | (int)buf[5];
            *out_event = TP_EVENT_UP;
            *out_x = x;
            *out_y = y;
            prev_touch = 0;
            return 0;
        }
        return -1;  /* no touch and wasn't touching — no event */
    }

    if (touch_num > FT6146_MAX_POINTS) {
        touch_num = FT6146_MAX_POINTS;
    }

    int x = (int)((buf[2] & 0x0FU) << 8) | (int)buf[3];
    int y = (int)((buf[4] & 0x0FU) << 8) | (int)buf[5];

    if (!prev_touch) {
        *out_event = TP_EVENT_DOWN;   /* transition 0→1: synthesize DOWN */
    } else {
        uint8_t event_flag = (buf[2] >> 6) & 0x03U;
        *out_event = (event_flag == TP_EVENT_UP) ? TP_EVENT_UP : TP_EVENT_MOVE;
    }

    *out_x = x;
    *out_y = y;
    prev_touch = (touch_num > 0) ? 1 : 0;

    return 0;
}

static uint32_t ft6146_read_id(const tp_bus_t *bus, uint8_t dev_addr)
{
    uint8_t id_h = 0, id_l = 0;
    if (bus->read(dev_addr, FT_REG_READ_ID_H, &id_h, 1U) == 0 &&
        bus->read(dev_addr, FT_REG_READ_ID_L, &id_l, 1U) == 0) {
        return ((uint32_t)id_h << 8) | id_l;
    }
    return 0;
}

/* ── IC vtable ────────────────────────────────────────────────────────── */

const tp_ic_t tp_ic_ft6146 = {
    .name       = "FT6146",
    .dev_addr   = FT_DEV_ADDR,
    .init       = ft6146_init,
    .read_touch = ft6146_read_touch,
    .read_id    = ft6146_read_id,
};
