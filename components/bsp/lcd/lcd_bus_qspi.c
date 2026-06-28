/**
 * @file lcd_bus_qspi.c
 * @brief QSPI bit-bang transport (CLK + D0-D3 + CS).
 *
 * Exports lcd_bus_qspi (vtable) for IC driver access, plus
 * lcd_bus_write_pixels / lcd_bus_fill_pixels (direct calls)
 * for lcd.c's drawing hot path.
 */

#include "lcd.h"
#include "SF32LB52.h"
#include "board.h"
#include "hal.h"

/* ═══════════════════════════════════════════════════════════════════════
 * Slow path — single-line via digitalWrite
 * ═══════════════════════════════════════════════════════════════════════ */

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

const lcd_bus_t lcd_bus_qspi = {
    .init      = qspi_init,
    .begin     = qspi_begin,
    .end       = qspi_end,
    .cmd_write = qspi_cmd_write,
    .cmd_read  = qspi_cmd_read,
};

/* ═══════════════════════════════════════════════════════════════════════
 * Fast pixel output — called by lcd.c via extern convention
 * ═══════════════════════════════════════════════════════════════════════ */

_Static_assert(LCD_D1 == LCD_D0 + 1 && LCD_D2 == LCD_D0 + 2 &&
               LCD_D3 == LCD_D0 + 3 && LCD_D0 >= 4,
               "push needs consecutive D0..D3 with D0>=4");

RAMFUNC __attribute__((noinline))
void lcd_send(const uint16_t *pixels, uint32_t n)
{
    HPSYS_GPIO_TypeDef *g = HPSYS_GPIO;
    const uint32_t dm  = 0xFUL << LCD_D0;
    const uint32_t ck  = 1UL << LCD_CLK;
    const uint32_t clr = dm | ck;

    qspi_begin();
    qspi_write_byte(0x12); qspi_write_byte4(0x00);
    qspi_write_byte4(0x2C); qspi_write_byte4(0x00);

    #define PUSH(px) do { uint32_t _c = (px); \
        g->DOCR0.R = clr; g->DOSR0.R = (_c >> (12 - LCD_D0)) & dm; g->DOSR0.R = ck; \
        g->DOCR0.R = clr; g->DOSR0.R = (_c >> (8  - LCD_D0)) & dm; g->DOSR0.R = ck; \
        g->DOCR0.R = clr; g->DOSR0.R = (_c << (LCD_D0 - 4))  & dm; g->DOSR0.R = ck; \
        g->DOCR0.R = clr; g->DOSR0.R = (_c << LCD_D0)        & dm; g->DOSR0.R = ck; \
    } while (0)

    const uint16_t *s = pixels;
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
}

RAMFUNC __attribute__((noinline))
void lcd_fill(uint16_t color, uint32_t n)
{
    HPSYS_GPIO_TypeDef *g = HPSYS_GPIO;
    const uint32_t ck = 1UL << LCD_CLK;
    const uint32_t dm = 0xFUL << LCD_D0;
    const uint8_t  hb = (uint8_t)(color >> 8), lb = (uint8_t)(color & 0xFF);

    qspi_begin();
    qspi_write_byte(0x12); qspi_write_byte4(0x00);
    qspi_write_byte4(0x2C); qspi_write_byte4(0x00);

    #define N(n,b) (((b) & (0x10U<<(n))) ? (1UL<<(LCD_D0+(n))) : 0U)
    const uint32_t hh_s = N(0,hb)|N(1,hb)|N(2,hb)|N(3,hb);
    const uint32_t hl_s = N(0,hb>>0)|N(1,hb>>0)|N(2,hb>>0)|N(3,hb>>0);
    const uint32_t lh_s = N(0,lb)|N(1,lb)|N(2,lb)|N(3,lb);
    const uint32_t ll_s = N(0,lb>>0)|N(1,lb>>0)|N(2,lb>>0)|N(3,lb>>0);
    #undef N
    const uint32_t hh_c = dm & ~hh_s, hl_c = dm & ~hl_s;
    const uint32_t lh_c = dm & ~lh_s, ll_c = dm & ~ll_s;

    for (uint32_t i = 0U; i < n; ++i) {
        g->DOCR0.R = hh_c | ck; g->DOSR0.R = hh_s; g->DOSR0.R = ck;
        g->DOCR0.R = hl_c | ck; g->DOSR0.R = hl_s; g->DOSR0.R = ck;
        g->DOCR0.R = lh_c | ck; g->DOSR0.R = lh_s; g->DOSR0.R = ck;
        g->DOCR0.R = ll_c | ck; g->DOSR0.R = ll_s; g->DOSR0.R = ck;
    }
    g->DOCR0.R = ck;
    qspi_end();
}
