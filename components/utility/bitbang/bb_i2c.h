#pragma once
#ifndef BB_I2C_H
#define BB_I2C_H

#include <stdint.h>

/**
 * @file bb_i2c.h
 * @brief Bit-banged I2C master (software open-drain emulation).
 *
 * Open-drain emulation: SDA high = pinMode(INPUT), SDA low = pinMode(OUTPUT)+digitalWrite(LOW).
 * The I2C address parameter is always a 7-bit address; this driver shifts it left by 1
 * and appends the R/W bit internally.
 */

typedef void (*bb_delay_fn)(void);

typedef struct {
    uint32_t    pin_sda;
    uint32_t    pin_scl;
    bb_delay_fn half_period;  /* optional: called once per half-bit */
} bb_i2c_t;

void    bb_i2c_init(const bb_i2c_t *dev);
void    bb_i2c_start(const bb_i2c_t *dev);
void    bb_i2c_stop(const bb_i2c_t *dev);
uint8_t bb_i2c_write_byte(const bb_i2c_t *dev, uint8_t data);
uint8_t bb_i2c_read_byte(const bb_i2c_t *dev, uint8_t ack);

int bb_i2c_write(const bb_i2c_t *dev, uint8_t addr, const uint8_t *data, uint16_t len);
int bb_i2c_read(const bb_i2c_t *dev, uint8_t addr, uint8_t *data, uint16_t len);
int bb_i2c_mem_write(const bb_i2c_t *dev, uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len);
int bb_i2c_mem_read(const bb_i2c_t *dev, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);

/**
 * @brief Pulse SCL 9 times with SDA high to recover a stuck bus.
 */
void bb_i2c_reset(const bb_i2c_t *dev);

/**
 * @brief Scan 7-bit addresses [1..127], call callback for each ACK.
 * @param cb  Called as cb(addr, user) for each responding device.
 */
typedef void (*bb_i2c_scan_cb)(uint8_t addr, void *user);
void bb_i2c_scan(const bb_i2c_t *dev, bb_i2c_scan_cb cb, void *user);

#endif
