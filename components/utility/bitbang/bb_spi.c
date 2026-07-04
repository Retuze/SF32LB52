#include "bb_spi.h"
#include "bb_gpio.h"

void bb_spi_init(const bb_spi_t *dev)
{
    uint8_t idle = (dev->mode & 0x02U) ? HIGH : LOW;

    pinMode(dev->pin_sck,  OUTPUT);
    pinMode(dev->pin_mosi, OUTPUT);
    pinMode(dev->pin_miso, INPUT);
    pinMode(dev->pin_cs,   OUTPUT);

    digitalWrite(dev->pin_sck,  idle);
    digitalWrite(dev->pin_mosi, LOW);
    digitalWrite(dev->pin_cs,   HIGH);
}

void bb_spi_cs_low(const bb_spi_t *dev)
{
    digitalWrite(dev->pin_cs, LOW);
}

void bb_spi_cs_high(const bb_spi_t *dev)
{
    digitalWrite(dev->pin_cs, HIGH);
}

uint8_t bb_spi_transfer(const bb_spi_t *dev, uint8_t tx)
{
    uint8_t cpol = (dev->mode & 0x02U) ? 1U : 0U;
    uint8_t cpha = (dev->mode & 0x01U) ? 1U : 0U;
    uint8_t rx = 0U;

    for (uint8_t i = 0U; i < 8U; ++i) {
        if (cpha == 0U) {
            digitalWrite(dev->pin_mosi, (tx & 0x80U) ? HIGH : LOW);
            tx <<= 1U;
            digitalWrite(dev->pin_sck, cpol ? LOW : HIGH);
            rx <<= 1U;
            if (digitalRead(dev->pin_miso) == HIGH) {
                rx |= 0x01U;
            }
            digitalWrite(dev->pin_sck, cpol ? HIGH : LOW);
        } else {
            digitalWrite(dev->pin_sck, cpol ? LOW : HIGH);
            digitalWrite(dev->pin_mosi, (tx & 0x80U) ? HIGH : LOW);
            tx <<= 1U;
            digitalWrite(dev->pin_sck, cpol ? HIGH : LOW);
            rx <<= 1U;
            if (digitalRead(dev->pin_miso) == HIGH) {
                rx |= 0x01U;
            }
        }
    }
    return rx;
}

void bb_spi_send(const bb_spi_t *dev, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        bb_spi_transfer(dev, data[i]);
    }
}

void bb_spi_recv(const bb_spi_t *dev, uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        data[i] = bb_spi_transfer(dev, 0xFFU);
    }
}
