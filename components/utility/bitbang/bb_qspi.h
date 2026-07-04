#pragma once
#ifndef BB_QSPI_H
#define BB_QSPI_H

#include <stdint.h>

/** GPIO bit-bang QSPI wiring (not an LCD device). */
typedef struct {
    uint32_t pin_clk;
    uint32_t pin_cs;
    uint32_t pin_d0;
    uint32_t pin_d1;
    uint32_t pin_d2;
    uint32_t pin_d3;
} bb_qspi_bus_t;

void    bb_qspi_init(const bb_qspi_bus_t *bus);
void    bb_qspi_cs_low(const bb_qspi_bus_t *bus);
void    bb_qspi_cs_high(const bb_qspi_bus_t *bus);
void    bb_qspi_send_byte(const bb_qspi_bus_t *bus, uint8_t data);
void    bb_qspi_send_byte_4wire(const bb_qspi_bus_t *bus, uint8_t data);
void    bb_qspi_send(const bb_qspi_bus_t *bus, const uint8_t *data, uint16_t len);
void    bb_qspi_send_4wire(const bb_qspi_bus_t *bus, const uint8_t *data, uint16_t len);
uint8_t bb_qspi_recv_byte(const bb_qspi_bus_t *bus);
void    bb_qspi_recv(const bb_qspi_bus_t *bus, uint8_t *data, uint16_t len);

#endif
