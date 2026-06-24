#pragma once
#ifndef BB_SPI_H
#define BB_SPI_H

#include <stdint.h>

#define BB_SPI_MODE_0  0
#define BB_SPI_MODE_1  1
#define BB_SPI_MODE_2  2
#define BB_SPI_MODE_3  3

typedef struct {
    uint32_t pin_sck;
    uint32_t pin_mosi;
    uint32_t pin_miso;
    uint32_t pin_cs;
    uint8_t  mode;
} bb_spi_t;

void    bb_spi_init(const bb_spi_t *dev);
void    bb_spi_cs_low(const bb_spi_t *dev);
void    bb_spi_cs_high(const bb_spi_t *dev);
uint8_t bb_spi_transfer(const bb_spi_t *dev, uint8_t tx);
void    bb_spi_send(const bb_spi_t *dev, const uint8_t *data, uint16_t len);
void    bb_spi_recv(const bb_spi_t *dev, uint8_t *data, uint16_t len);

#endif
