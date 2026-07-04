/**
 * @file tp_bus_i2c.c
 * @brief TP I2C bus driver — bit-bang I2C transport layer.
 */

#include "tp.h"
#include "bb_i2c.h"

/* ── I2C transport layer ──────────────────────────────────────────────── */

static void i2c_init(const void *config)
{
    const bb_i2c_t *i2c = (const bb_i2c_t *)config;
    bb_i2c_init(i2c);
}

static int i2c_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    /* For I2C bus, we need access to the bb_i2c_t* stored in tp.c's global state.
     * Since we can't pass it through this interface cleanly, we'll use a static
     * pointer set during init. */
    extern const void *_tp_bus_config_ptr;  /* defined in tp.c */
    const bb_i2c_t *i2c = (const bb_i2c_t *)_tp_bus_config_ptr;
    return bb_i2c_mem_read(i2c, dev_addr, reg, data, len);
}

static int i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    extern const void *_tp_bus_config_ptr;
    const bb_i2c_t *i2c = (const bb_i2c_t *)_tp_bus_config_ptr;
    return bb_i2c_mem_write(i2c, dev_addr, reg, data, len);
}

/* ── Bus vtable ───────────────────────────────────────────────────────── */

const tp_bus_t tp_bus_i2c = {
    .init  = i2c_init,
    .read  = i2c_read,
    .write = i2c_write,
};
