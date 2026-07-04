/**
 * @file tp_bus_i2c.c
 * @brief TP I2C bus driver — bit-bang I2C with pins from board.h.
 *
 * Uses CTP_SDA/CTP_SCL from board.h directly (no runtime config).
 * Open-drain emulation: SDA/SCL high = INPUT, low = OUTPUT+LOW.
 */

#include "tp.h"
#include "board.h"
#include "ll_gpio.h"

/* ── GPIO primitives (open-drain emulation) ─────────────────────────────── */

static inline void sda_high(void) { pinMode(CTP_SDA, INPUT); }
static inline void sda_low(void)  { pinMode(CTP_SDA, OUTPUT); digitalWrite(CTP_SDA, LOW); }
static inline void scl_high(void) { pinMode(CTP_SCL, INPUT); }
static inline void scl_low(void)  { pinMode(CTP_SCL, OUTPUT); digitalWrite(CTP_SCL, LOW); }
static inline uint8_t sda_read(void) { return digitalRead(CTP_SDA); }

/* I2C half-period delay (~250 kHz clock) */
static inline void half_delay(void)
{
    for (volatile uint32_t i = 0; i < 40; ++i) {
        __asm volatile("nop");
    }
}

/* ── I2C protocol primitives ────────────────────────────────────────────── */

static void i2c_start(void)
{
    sda_high();
    scl_high();
    half_delay();
    sda_low();
    half_delay();
    scl_low();
}

static void i2c_stop(void)
{
    sda_low();
    half_delay();
    scl_high();
    half_delay();
    sda_high();
    half_delay();
}

static uint8_t i2c_write_byte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; ++i) {
        if (data & 0x80) {
            sda_high();
        } else {
            sda_low();
        }
        data <<= 1;
        half_delay();
        scl_high();
        half_delay();
        scl_low();
    }

    /* Read ACK bit */
    sda_high();
    half_delay();
    scl_high();
    half_delay();
    uint8_t ack = (sda_read() == LOW) ? 1 : 0;
    scl_low();
    return ack;
}

static uint8_t i2c_read_byte(uint8_t send_ack)
{
    uint8_t data = 0;
    sda_high();

    for (uint8_t i = 0; i < 8; ++i) {
        data <<= 1;
        half_delay();
        scl_high();
        half_delay();
        if (sda_read() == HIGH) {
            data |= 0x01;
        }
        scl_low();
    }

    /* Send ACK/NACK */
    if (send_ack) {
        sda_low();
    } else {
        sda_high();
    }
    half_delay();
    scl_high();
    half_delay();
    scl_low();
    sda_high();

    return data;
}

/* ── I2C memory transactions (register read/write) ──────────────────────── */

static int i2c_mem_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    i2c_start();
    if (!i2c_write_byte(dev_addr << 1)) goto fail;
    if (!i2c_write_byte(reg)) goto fail;
    for (uint16_t i = 0; i < len; ++i) {
        if (!i2c_write_byte(data[i])) goto fail;
    }
    i2c_stop();
    return 0;
fail:
    i2c_stop();
    return -1;
}

static int i2c_mem_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    i2c_start();
    if (!i2c_write_byte(dev_addr << 1)) goto fail;
    if (!i2c_write_byte(reg)) goto fail;

    i2c_start();  /* Repeated start */
    if (!i2c_write_byte((dev_addr << 1) | 1)) goto fail;
    for (uint16_t i = 0; i < len; ++i) {
        data[i] = i2c_read_byte((i < len - 1) ? 1 : 0);
    }
    i2c_stop();
    return 0;
fail:
    i2c_stop();
    return -1;
}

/* ── Bus layer interface ────────────────────────────────────────────────── */

static void i2c_init(void)
{
    sda_high();
    scl_high();
}

static int i2c_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    return i2c_mem_read(dev_addr, reg, data, len);
}

static int i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    return i2c_mem_write(dev_addr, reg, data, len);
}

/* ── Bus vtable ─────────────────────────────────────────────────────────── */

const tp_bus_t tp_bus_i2c = {
    .init  = i2c_init,
    .read  = i2c_read,
    .write = i2c_write,
};
