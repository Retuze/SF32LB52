#include "bb_uart.h"
#include "bb_gpio.h"

static void bit_wait(const bb_uart_t *dev)
{
    if (dev->bit_delay) {
        dev->bit_delay();
    }
}

void bb_uart_init(const bb_uart_t *dev)
{
    pinMode(dev->pin_tx, OUTPUT);
    digitalWrite(dev->pin_tx, HIGH);
    pinMode(dev->pin_rx, INPUT);
}

void bb_uart_send_byte(const bb_uart_t *dev, uint8_t data)
{
    /* start bit */
    digitalWrite(dev->pin_tx, LOW);
    bit_wait(dev);

    /* 8 data bits, LSB first */
    for (uint8_t i = 0U; i < 8U; ++i) {
        digitalWrite(dev->pin_tx, (data & 0x01U) ? HIGH : LOW);
        data >>= 1U;
        bit_wait(dev);
    }

    /* stop bit */
    digitalWrite(dev->pin_tx, HIGH);
    bit_wait(dev);
}

void bb_uart_send(const bb_uart_t *dev, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        bb_uart_send_byte(dev, data[i]);
    }
}

uint8_t bb_uart_recv_byte(const bb_uart_t *dev)
{
    uint8_t data = 0U;

    while (digitalRead(dev->pin_rx) == HIGH) {
    }

    /* half-bit to sample at center */
    bit_wait(dev);

    for (uint8_t i = 0U; i < 8U; ++i) {
        bit_wait(dev);
        if (digitalRead(dev->pin_rx) == HIGH) {
            data |= (uint8_t)(1U << i);
        }
    }

    bit_wait(dev);
    return data;
}
