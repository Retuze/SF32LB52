#pragma once
#ifndef BB_I2C_H
#define BB_I2C_H

#include <stdint.h>

typedef void (*bb_delay_fn)(void);

typedef struct {
    uint32_t    pin_sda;
    uint32_t    pin_scl;
    bb_delay_fn half_period;
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

#endif
