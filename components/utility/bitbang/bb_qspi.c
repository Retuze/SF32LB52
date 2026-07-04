#include "bb_qspi.h"
#include "bb_gpio.h"

void bb_qspi_init(const bb_qspi_bus_t *bus)
{
    pinMode(bus->pin_clk, OUTPUT);
    pinMode(bus->pin_cs, OUTPUT);
    pinMode(bus->pin_d0, OUTPUT);
    pinMode(bus->pin_d1, OUTPUT);
    pinMode(bus->pin_d2, OUTPUT);
    pinMode(bus->pin_d3, OUTPUT);

    digitalWrite(bus->pin_clk, LOW);
    digitalWrite(bus->pin_cs, HIGH);
    digitalWrite(bus->pin_d0, LOW);
    digitalWrite(bus->pin_d1, LOW);
    digitalWrite(bus->pin_d2, LOW);
    digitalWrite(bus->pin_d3, LOW);
}

void bb_qspi_cs_low(const bb_qspi_bus_t *bus)
{
    digitalWrite(bus->pin_cs, LOW);
}

void bb_qspi_cs_high(const bb_qspi_bus_t *bus)
{
    digitalWrite(bus->pin_cs, HIGH);
}

void bb_qspi_send_byte(const bb_qspi_bus_t *bus, uint8_t data)
{
    digitalWrite(bus->pin_clk, LOW);
    for (uint8_t i = 0U; i < 8U; ++i) {
        digitalWrite(bus->pin_d0, (data & 0x80U) ? HIGH : LOW);
        digitalWrite(bus->pin_clk, HIGH);
        digitalWrite(bus->pin_clk, LOW);
        data <<= 1U;
    }
}

void bb_qspi_send_byte_4wire(const bb_qspi_bus_t *bus, uint8_t data)
{
    digitalWrite(bus->pin_clk, LOW);

    digitalWrite(bus->pin_d0, (data & 0x10U) ? HIGH : LOW);
    digitalWrite(bus->pin_d1, (data & 0x20U) ? HIGH : LOW);
    digitalWrite(bus->pin_d2, (data & 0x40U) ? HIGH : LOW);
    digitalWrite(bus->pin_d3, (data & 0x80U) ? HIGH : LOW);
    digitalWrite(bus->pin_clk, HIGH);
    digitalWrite(bus->pin_clk, LOW);

    digitalWrite(bus->pin_d0, (data & 0x01U) ? HIGH : LOW);
    digitalWrite(bus->pin_d1, (data & 0x02U) ? HIGH : LOW);
    digitalWrite(bus->pin_d2, (data & 0x04U) ? HIGH : LOW);
    digitalWrite(bus->pin_d3, (data & 0x08U) ? HIGH : LOW);
    digitalWrite(bus->pin_clk, HIGH);
    digitalWrite(bus->pin_clk, LOW);
}

void bb_qspi_send(const bb_qspi_bus_t *bus, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        bb_qspi_send_byte(bus, data[i]);
    }
}

void bb_qspi_send_4wire(const bb_qspi_bus_t *bus, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        bb_qspi_send_byte_4wire(bus, data[i]);
    }
}

uint8_t bb_qspi_recv_byte(const bb_qspi_bus_t *bus)
{
    uint8_t data = 0U;

    digitalWrite(bus->pin_clk, LOW);
    for (uint8_t i = 0U; i < 8U; ++i) {
        data <<= 1U;
        digitalWrite(bus->pin_clk, HIGH);
        if (digitalRead(bus->pin_d0) == HIGH) {
            data |= 0x01U;
        }
        digitalWrite(bus->pin_clk, LOW);
    }
    return data;
}

void bb_qspi_recv(const bb_qspi_bus_t *bus, uint8_t *data, uint16_t len)
{
    pinMode(bus->pin_d0, INPUT);
    for (uint16_t i = 0U; i < len; ++i) {
        data[i] = bb_qspi_recv_byte(bus);
    }
    pinMode(bus->pin_d0, OUTPUT);
}
