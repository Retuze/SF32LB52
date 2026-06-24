#include "bb_i2c.h"
#include "bb_gpio.h"

static void sda_high(const bb_i2c_t *dev) { pinMode(dev->pin_sda, INPUT); }
static void sda_low(const bb_i2c_t *dev)  { pinMode(dev->pin_sda, OUTPUT); digitalWrite(dev->pin_sda, LOW); }
static void scl_high(const bb_i2c_t *dev) { pinMode(dev->pin_scl, INPUT); }
static void scl_low(const bb_i2c_t *dev)  { pinMode(dev->pin_scl, OUTPUT); digitalWrite(dev->pin_scl, LOW); }
static uint8_t sda_read(const bb_i2c_t *dev) { return digitalRead(dev->pin_sda); }

static void half_delay(const bb_i2c_t *dev)
{
    if (dev->half_period) {
        dev->half_period();
    }
}

void bb_i2c_init(const bb_i2c_t *dev)
{
    sda_high(dev);
    scl_high(dev);
}

void bb_i2c_start(const bb_i2c_t *dev)
{
    sda_high(dev);
    scl_high(dev);
    half_delay(dev);
    sda_low(dev);
    half_delay(dev);
    scl_low(dev);
}

void bb_i2c_stop(const bb_i2c_t *dev)
{
    sda_low(dev);
    half_delay(dev);
    scl_high(dev);
    half_delay(dev);
    sda_high(dev);
    half_delay(dev);
}

uint8_t bb_i2c_write_byte(const bb_i2c_t *dev, uint8_t data)
{
    for (uint8_t i = 0U; i < 8U; ++i) {
        if (data & 0x80U) {
            sda_high(dev);
        } else {
            sda_low(dev);
        }
        data <<= 1U;
        half_delay(dev);
        scl_high(dev);
        half_delay(dev);
        scl_low(dev);
    }

    sda_high(dev);
    half_delay(dev);
    scl_high(dev);
    half_delay(dev);
    uint8_t ack = (sda_read(dev) == LOW) ? 1U : 0U;
    scl_low(dev);
    return ack;
}

uint8_t bb_i2c_read_byte(const bb_i2c_t *dev, uint8_t ack)
{
    uint8_t data = 0U;
    sda_high(dev);

    for (uint8_t i = 0U; i < 8U; ++i) {
        data <<= 1U;
        half_delay(dev);
        scl_high(dev);
        half_delay(dev);
        if (sda_read(dev) == HIGH) {
            data |= 0x01U;
        }
        scl_low(dev);
    }

    if (ack) {
        sda_low(dev);
    } else {
        sda_high(dev);
    }
    half_delay(dev);
    scl_high(dev);
    half_delay(dev);
    scl_low(dev);
    sda_high(dev);

    return data;
}

int bb_i2c_write(const bb_i2c_t *dev, uint8_t addr, const uint8_t *data, uint16_t len)
{
    bb_i2c_start(dev);
    if (!bb_i2c_write_byte(dev, (uint8_t)(addr << 1U))) {
        bb_i2c_stop(dev);
        return -1;
    }
    for (uint16_t i = 0U; i < len; ++i) {
        if (!bb_i2c_write_byte(dev, data[i])) {
            bb_i2c_stop(dev);
            return -1;
        }
    }
    bb_i2c_stop(dev);
    return 0;
}

int bb_i2c_read(const bb_i2c_t *dev, uint8_t addr, uint8_t *data, uint16_t len)
{
    bb_i2c_start(dev);
    if (!bb_i2c_write_byte(dev, (uint8_t)((addr << 1U) | 1U))) {
        bb_i2c_stop(dev);
        return -1;
    }
    for (uint16_t i = 0U; i < len; ++i) {
        data[i] = bb_i2c_read_byte(dev, (i < (uint16_t)(len - 1U)) ? 1U : 0U);
    }
    bb_i2c_stop(dev);
    return 0;
}

int bb_i2c_mem_write(const bb_i2c_t *dev, uint8_t addr, uint8_t reg,
                     const uint8_t *data, uint16_t len)
{
    bb_i2c_start(dev);
    if (!bb_i2c_write_byte(dev, (uint8_t)(addr << 1U))) goto fail;
    if (!bb_i2c_write_byte(dev, reg)) goto fail;
    for (uint16_t i = 0U; i < len; ++i) {
        if (!bb_i2c_write_byte(dev, data[i])) goto fail;
    }
    bb_i2c_stop(dev);
    return 0;
fail:
    bb_i2c_stop(dev);
    return -1;
}

int bb_i2c_mem_read(const bb_i2c_t *dev, uint8_t addr, uint8_t reg,
                    uint8_t *data, uint16_t len)
{
    bb_i2c_start(dev);
    if (!bb_i2c_write_byte(dev, (uint8_t)(addr << 1U))) goto fail;
    if (!bb_i2c_write_byte(dev, reg)) goto fail;

    bb_i2c_start(dev);
    if (!bb_i2c_write_byte(dev, (uint8_t)((addr << 1U) | 1U))) goto fail;
    for (uint16_t i = 0U; i < len; ++i) {
        data[i] = bb_i2c_read_byte(dev, (i < (uint16_t)(len - 1U)) ? 1U : 0U);
    }
    bb_i2c_stop(dev);
    return 0;
fail:
    bb_i2c_stop(dev);
    return -1;
}
