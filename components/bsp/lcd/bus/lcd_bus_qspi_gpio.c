/**
 * @file lcd_bus_qspi_gpio.c
 * @brief QSPI bit-bang transport via GPIO (CLK + D0-D3 + CS).
 *
 * Defines lcd_bus_default for IC register access (slow path, digitalWrite)
 * and lcd_send / lcd_fill for pixel data (fast path, direct GPIO registers).
 */

#include <stdio.h>
#include "lcd.h"
#include "SF32LB52.h"
#include "board.h"
#include "hal.h"

/* ── Slow path — register I/O via digitalWrite ────────────────────────── */

static void qspi_init(void)
{
    pinMode(LCD_CLK, OUTPUT); digitalWrite(LCD_CLK, LOW);
    pinMode(LCD_CS,  OUTPUT); digitalWrite(LCD_CS,  HIGH);
    pinMode(LCD_D0,  OUTPUT); digitalWrite(LCD_D0,  LOW);
    pinMode(LCD_D1,  OUTPUT); digitalWrite(LCD_D1,  LOW);
    pinMode(LCD_D2,  OUTPUT); digitalWrite(LCD_D2,  LOW);
    pinMode(LCD_D3,  OUTPUT); digitalWrite(LCD_D3,  LOW);
}

static void qspi_begin(void)  { digitalWrite(LCD_CS, LOW); }
static void qspi_end(void)    { digitalWrite(LCD_CS, HIGH); }

static void qspi_write_byte(uint8_t data)
{
    digitalWrite(LCD_CLK, LOW);
    for (uint8_t i = 0U; i < 8U; ++i) {
        digitalWrite(LCD_D0, (data & 0x80U) ? HIGH : LOW);
        digitalWrite(LCD_CLK, HIGH);
        digitalWrite(LCD_CLK, LOW);
        data <<= 1U;
    }
}

static void qspi_write_byte4(uint8_t data)
{
    digitalWrite(LCD_CLK, LOW);
    digitalWrite(LCD_D0, (data & 0x10U) ? HIGH : LOW);
    digitalWrite(LCD_D1, (data & 0x20U) ? HIGH : LOW);
    digitalWrite(LCD_D2, (data & 0x40U) ? HIGH : LOW);
    digitalWrite(LCD_D3, (data & 0x80U) ? HIGH : LOW);
    digitalWrite(LCD_CLK, HIGH);
    digitalWrite(LCD_CLK, LOW);
    digitalWrite(LCD_D0, (data & 0x01U) ? HIGH : LOW);
    digitalWrite(LCD_D1, (data & 0x02U) ? HIGH : LOW);
    digitalWrite(LCD_D2, (data & 0x04U) ? HIGH : LOW);
    digitalWrite(LCD_D3, (data & 0x08U) ? HIGH : LOW);
    digitalWrite(LCD_CLK, HIGH);
    digitalWrite(LCD_CLK, LOW);
}

static void qspi_cmd_write(uint8_t cmd, const uint8_t *param, uint32_t param_len)
{
    qspi_write_byte(0x02); qspi_write_byte(0x00);
    qspi_write_byte(cmd);  qspi_write_byte(0x00);
    for (uint32_t i = 0U; param && i < param_len; ++i)
        qspi_write_byte(param[i]);
}

static void qspi_cmd_read(uint8_t cmd, uint8_t *data, uint32_t data_len)
{
    qspi_write_byte(0x03); qspi_write_byte(0x00);
    qspi_write_byte(cmd);  qspi_write_byte(0x00);
    if (data && data_len) {
        pinMode(LCD_D0, INPUT);
        for (uint32_t i = 0U; i < data_len; ++i) {
            uint8_t v = 0U;
            digitalWrite(LCD_CLK, LOW);
            for (uint8_t b = 0U; b < 8U; ++b) {
                v <<= 1U;
                digitalWrite(LCD_CLK, HIGH);
                if (digitalRead(LCD_D0)) v |= 0x01U;
                digitalWrite(LCD_CLK, LOW);
            }
            data[i] = v;
        }
        pinMode(LCD_D0, OUTPUT);
    }
}

const lcd_bus_t lcd_bus_qspi_gpio = {
    .init      = qspi_init,
    .begin     = qspi_begin,
    .end       = qspi_end,
    .send      = qspi_cmd_write,
    .read      = qspi_cmd_read,
    .set_speed = NULL,  /* fixed speed, determined by bit-bang timing */
};

/* ── Fast lcd_bitblt via direct GPIO registers ──────────────────────── */

_Static_assert(LCD_D1 == LCD_D0 + 1 && LCD_D2 == LCD_D0 + 2 &&
               LCD_D3 == LCD_D0 + 3 && LCD_D0 >= 4,
               "push needs consecutive D0..D3 with D0>=4");

void lcd_bitblt(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                const uint16_t *rgb565,
                void (*done)(void *ctx), void *ctx)
{
    if (!rgb565 || !w || !h) {
        if (done) done(ctx);
        return;
    }

    extern void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    lcd_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

    HPSYS_GPIO_TypeDef *g = HPSYS_GPIO;
    const uint32_t dm  = 0xFUL << LCD_D0;
    const uint32_t ck  = 1UL << LCD_CLK;
    const uint32_t clr = dm | ck;

    qspi_begin();
    // 0x12 = quad-cmd + quad-data (consistent with LCDC driver)
    qspi_write_byte(0x12); qspi_write_byte4(0x00);
    qspi_write_byte4(0x2C); qspi_write_byte4(0x00);

    #define PUSH(px) do { uint32_t _c = (px); \
        g->DOCR0.R = clr; g->DOSR0.R = (_c >> (12 - LCD_D0)) & dm; g->DOSR0.R = ck; \
        g->DOCR0.R = clr; g->DOSR0.R = (_c >> (8  - LCD_D0)) & dm; g->DOSR0.R = ck; \
        g->DOCR0.R = clr; g->DOSR0.R = (_c << (LCD_D0 - 4))  & dm; g->DOSR0.R = ck; \
        g->DOCR0.R = clr; g->DOSR0.R = (_c << LCD_D0)        & dm; g->DOSR0.R = ck; \
    } while (0)

    const uint16_t *s = rgb565;
    uint32_t n = (uint32_t)w * (uint32_t)h;
    while (n >= 4U) {
        uint32_t a = *(const uint32_t *)(const void *)s;
        uint32_t b = *(const uint32_t *)(const void *)(s + 2);
        PUSH(a & 0xFFFFU); PUSH(a >> 16);
        PUSH(b & 0xFFFFU); PUSH(b >> 16);
        s += 4; n -= 4U;
    }
    while (n--) PUSH((uint32_t)*s++);
    #undef PUSH

    g->DOCR0.R = ck;
    qspi_end();

    if (done) done(ctx);
}

/* Force this object file to be linked (ensures strong lcd_bitblt overrides weak default) */
__attribute__((used))
static const void *force_link_gpio_bus = &lcd_bus_qspi_gpio;
