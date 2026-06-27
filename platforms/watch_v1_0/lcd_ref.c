/**
 * @file lcd_ref.c
 * @brief Verbatim port of the reference SDK lcd.c + bb_qspi.c.
 *
 * Adapted minimally: includes use hal headers, `print/println` → `printf`,
 * DWT helpers are local stubs (SystemInit already enables the cycle counter).
 */
#include "lcd_ref.h"

#include "SF32LB52.h"
#include "board.h"
#include "hal.h"
#include "hal_gpio.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* =========================================================================
 * QSPI bit-bang (from reference common/bit-bang/qspi/bb_qspi.c)
 * ========================================================================= */

static void qspi_init(void)
{
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    pinMode(LCD_CLK, OUTPUT);
    pinMode(LCD_CS, OUTPUT);
    pinMode(LCD_D0, OUTPUT);
    pinMode(LCD_D1, OUTPUT);
    pinMode(LCD_D2, OUTPUT);
    pinMode(LCD_D3, OUTPUT);

    digitalWrite(LCD_CLK, LOW);
    digitalWrite(LCD_CS, HIGH);
    digitalWrite(LCD_D0, LOW);
    digitalWrite(LCD_D1, LOW);
    digitalWrite(LCD_D2, LOW);
    digitalWrite(LCD_D3, LOW);
}

static void qspi_cmd_start(void) { digitalWrite(LCD_CS, LOW); }
static void qspi_cmd_end(void)   { digitalWrite(LCD_CS, HIGH); }

static void qspi_send_byte(uint8_t data)
{
    digitalWrite(LCD_CLK, LOW);
    for (uint8_t i = 0U; i < 8U; ++i) {
        digitalWrite(LCD_D0, (data & 0x80U) ? HIGH : LOW);
        digitalWrite(LCD_CLK, HIGH);
        digitalWrite(LCD_CLK, LOW);
        data <<= 1U;
    }
}

static void qspi_send_byte_4wire(uint8_t data)
{
    digitalWrite(LCD_CLK, LOW);

    /* Nibble 1: bits 7-4 */
    digitalWrite(LCD_D0, (data & 0x10U) ? HIGH : LOW);
    digitalWrite(LCD_D1, (data & 0x20U) ? HIGH : LOW);
    digitalWrite(LCD_D2, (data & 0x40U) ? HIGH : LOW);
    digitalWrite(LCD_D3, (data & 0x80U) ? HIGH : LOW);
    digitalWrite(LCD_CLK, HIGH);
    digitalWrite(LCD_CLK, LOW);

    /* Nibble 0: bits 3-0 */
    digitalWrite(LCD_D0, (data & 0x01U) ? HIGH : LOW);
    digitalWrite(LCD_D1, (data & 0x02U) ? HIGH : LOW);
    digitalWrite(LCD_D2, (data & 0x04U) ? HIGH : LOW);
    digitalWrite(LCD_D3, (data & 0x08U) ? HIGH : LOW);
    digitalWrite(LCD_CLK, HIGH);
    digitalWrite(LCD_CLK, LOW);
}

static void qspi_send_data(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) { qspi_send_byte(data[i]); }
}

static void qspi_send_data_4wire(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) { qspi_send_byte_4wire(data[i]); }
}

static uint8_t qspi_read_byte(void)
{
    uint8_t data = 0U;
    digitalWrite(LCD_CLK, LOW);
    for (uint8_t i = 0U; i < 8U; ++i) {
        data <<= 1U;
        digitalWrite(LCD_CLK, HIGH);
        if (digitalRead(LCD_D0) == HIGH) { data |= 0x01U; }
        digitalWrite(LCD_CLK, LOW);
    }
    return data;
}

static void qspi_read_data(uint8_t *data, uint16_t len)
{
    pinMode(LCD_D0, INPUT);
    for (uint16_t i = 0U; i < len; ++i) { data[i] = qspi_read_byte(); }
    pinMode(LCD_D0, OUTPUT);
}

/* =========================================================================
 * LCD driver (from reference partfrom/SF32LB52x/peripherals/lcd/lcd.c)
 * ========================================================================= */

#define LCD_CMD_SLPOUT 0x11
#define LCD_CMD_DISPON 0x29
#define LCD_CMD_CASET  0x2A
#define LCD_CMD_RASET  0x2B
#define LCD_CMD_RAMWR  0x2C

/* ---- DWT helpers (from ll_dwt.h via hal.h) ---------------------------- */
/* (dwt_set_us / dwt_get_us are already provided by ll_dwt.c) */

static SF32_RAMFUNC __attribute__((noinline)) void
lcd_push_pixels_ram(HPSYS_GPIO_TypeDef *gpio, const uint16_t *src,
                    uint32_t pixel_count, uint32_t clk_mask,
                    uint32_t d0_mask, uint32_t d1_mask,
                    uint32_t d2_mask, uint32_t d3_mask, uint32_t data_mask)
{
    // LCD_D0..D3 are consecutive GPIO bits, so a nibble maps onto the data
    // lines with a single shift — no per-nibble lookup table. And clr is a
    // constant (clear all data + clk; the following DOSR sets the wanted bits
    // back), so the inner loop is pure shift/mask + stores, zero table loads.
    _Static_assert(LCD_D1 == LCD_D0 + 1 && LCD_D2 == LCD_D0 + 2 &&
                   LCD_D3 == LCD_D0 + 3 && LCD_D0 >= 4,
                   "optimized push needs consecutive D0..D3 with D0>=4");
    (void)d0_mask; (void)d1_mask; (void)d2_mask; (void)d3_mask;

    const uint32_t dm  = data_mask;             // 0xF << LCD_D0
    const uint32_t clr = data_mask | clk_mask;  // clear data + clk (constant)
    const uint32_t ck  = clk_mask;

    // 4-pixel unrolled with 32-bit source loads — amortizes loop overhead and
    // halves the source loads. Each nibble keeps 3 writes (data, then constant
    // clk-high) so the store pipeline stays intact and data setup is preserved.
    #define PUSH(px) do { uint32_t _c = (px); \
        gpio->DOCR0.R = clr; gpio->DOSR0.R = (_c >> (12 - LCD_D0)) & dm; gpio->DOSR0.R = ck; \
        gpio->DOCR0.R = clr; gpio->DOSR0.R = (_c >> (8  - LCD_D0)) & dm; gpio->DOSR0.R = ck; \
        gpio->DOCR0.R = clr; gpio->DOSR0.R = (_c << (LCD_D0 - 4))  & dm; gpio->DOSR0.R = ck; \
        gpio->DOCR0.R = clr; gpio->DOSR0.R = (_c << LCD_D0)        & dm; gpio->DOSR0.R = ck; \
    } while (0)

    uint32_t n = pixel_count;
    const uint16_t* s = src;
    while (n >= 4U) {
        uint32_t a = *(const uint32_t*)(const void*)s;        // px 0,1
        uint32_t b = *(const uint32_t*)(const void*)(s + 2);  // px 2,3
        PUSH(a & 0xFFFFU); PUSH(a >> 16);
        PUSH(b & 0xFFFFU); PUSH(b >> 16);
        s += 4; n -= 4U;
    }
    while (n--) { PUSH((uint32_t)*s); ++s; }
    #undef PUSH

    gpio->DOCR0.R = clk_mask;
}

/* ---- Fast pixel push (ramfunc) — single colour ------------------------ */
static SF32_RAMFUNC __attribute__((noinline)) void
lcd_push_color_pixels_ram(HPSYS_GPIO_TypeDef *gpio, uint32_t pixel_count,
                          uint32_t clk_mask, uint32_t hb_hi_set,
                          uint32_t hb_hi_clr, uint32_t hb_lo_set,
                          uint32_t hb_lo_clr, uint32_t lb_hi_set,
                          uint32_t lb_hi_clr, uint32_t lb_lo_set,
                          uint32_t lb_lo_clr)
{
    for (uint32_t i = 0U; i < pixel_count; ++i) {
        gpio->DOCR0.R = hb_hi_clr | clk_mask;
        gpio->DOSR0.R = hb_hi_set;
        gpio->DOSR0.R = clk_mask;

        gpio->DOCR0.R = hb_lo_clr | clk_mask;
        gpio->DOSR0.R = hb_lo_set;
        gpio->DOSR0.R = clk_mask;

        gpio->DOCR0.R = lb_hi_clr | clk_mask;
        gpio->DOSR0.R = lb_hi_set;
        gpio->DOSR0.R = clk_mask;

        gpio->DOCR0.R = lb_lo_clr | clk_mask;
        gpio->DOSR0.R = lb_lo_set;
        gpio->DOSR0.R = clk_mask;
    }
    gpio->DOCR0.R = clk_mask;
}

/* ---- QSPI framing helpers ---------------------------------------------- */
static void lcd_write_cmd_02(uint8_t addr)
{
    qspi_cmd_end();
    qspi_cmd_start();
    qspi_send_byte(0x02);
    qspi_send_byte(0x00);
    qspi_send_byte(addr);
    qspi_send_byte(0x00);
}

static void lcd_write_cmd_12(uint8_t addr)
{
    qspi_cmd_end();
    qspi_cmd_start();
    qspi_send_byte(0x12);
    qspi_send_byte_4wire(0x00);
    qspi_send_byte_4wire(addr);
    qspi_send_byte_4wire(0x00);
}

static void lcd_write_cmd(uint8_t cmd) { lcd_write_cmd_02(cmd); }
static void lcd_write_data(uint8_t data) { qspi_send_byte(data); }

static void lcd_read_data_03(uint8_t addr)
{
    qspi_cmd_end();
    qspi_cmd_start();
    qspi_send_byte(0x03);
    qspi_send_byte(0x00);
    qspi_send_byte(addr);
    qspi_send_byte(0x00);
}

#define lcd_qspi_cmd_param(cmd, ...)                                           \
    do {                                                                       \
        lcd_write_cmd_02(cmd);                                                 \
        uint8_t _data[] = {__VA_ARGS__};                                       \
        for (size_t _i = 0U; _i < sizeof(_data) / sizeof(_data[0]); _i++) {   \
            lcd_write_data(_data[_i]);                                         \
        }                                                                      \
        qspi_cmd_end();                                                        \
    } while (0)

/* ---- Public API -------------------------------------------------------- */

void lcd_ref_reset(void)
{
    pinMode(LCD_RST, OUTPUT);
    digitalWrite(LCD_RST, HIGH);
    delay(10);
    digitalWrite(LCD_RST, LOW);
    delay(10);
    digitalWrite(LCD_RST, HIGH);
    delay(50);
}

void lcd_ref_read_id(uint8_t id[3])
{
    if (id == NULL) return;
    lcd_read_data_03(0x04);
    qspi_read_data(id, 3U);
    qspi_cmd_end();
}

int lcd_ref_init(void)
{
    uint8_t id[3] = {0U, 0U, 0U};

    qspi_init();
    lcd_ref_reset();

    lcd_ref_read_id(id);
    printf("LCD ID: %02X %02X %02X\n",
           (unsigned int)id[0], (unsigned int)id[1], (unsigned int)id[2]);

    delay(50);

    /* ---- IC init sequence (CO5300) ---- */
    lcd_qspi_cmd_param(0xFE, 0x20);
    lcd_qspi_cmd_param(0xF4, 0x5A);
    lcd_qspi_cmd_param(0xF5, 0x59);
    lcd_qspi_cmd_param(0xFE, 0x20);
    lcd_qspi_cmd_param(0xF4, 0xA5);
    lcd_qspi_cmd_param(0xF5, 0xA5);
    lcd_qspi_cmd_param(0xFE, 0x00);
    lcd_qspi_cmd_param(0xC4, 0x80);
    lcd_qspi_cmd_param(0x3A, 0x55);
    lcd_qspi_cmd_param(0x35, 0x00);
    lcd_qspi_cmd_param(0x53, 0x20);
    lcd_qspi_cmd_param(0x51, 0x80);
    lcd_qspi_cmd_param(0x63, 0xFF);

    lcd_qspi_cmd_param(LCD_CMD_CASET,
                       0x00, 0x00,
                       (uint8_t)((LCD_WIDTH_REF - 1U) >> 8),
                       (uint8_t)((LCD_WIDTH_REF - 1U) & 0xFFU));
    lcd_qspi_cmd_param(LCD_CMD_RASET,
                       0x00, 0x00,
                       (uint8_t)((LCD_HEIGHT_REF - 1U) >> 8),
                       (uint8_t)((LCD_HEIGHT_REF - 1U) & 0xFFU));

    lcd_write_cmd(LCD_CMD_SLPOUT);
    qspi_cmd_end();
    delay(120);

    lcd_write_cmd(LCD_CMD_DISPON);
    qspi_cmd_end();

    printf("LCD init done\n");
    return 0;
}

void lcd_ref_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_write_cmd(LCD_CMD_CASET);
    lcd_write_data((uint8_t)(x0 >> 8));
    lcd_write_data((uint8_t)(x0 & 0xFFU));
    lcd_write_data((uint8_t)(x1 >> 8));
    lcd_write_data((uint8_t)(x1 & 0xFFU));
    qspi_cmd_end();

    lcd_write_cmd(LCD_CMD_RASET);
    lcd_write_data((uint8_t)(y0 >> 8));
    lcd_write_data((uint8_t)(y0 & 0xFFU));
    lcd_write_data((uint8_t)(y1 >> 8));
    lcd_write_data((uint8_t)(y1 & 0xFFU));
    qspi_cmd_end();
}

void lcd_ref_fill_color(uint16_t color)
{
    lcd_ref_fill_rect(0U, 0U, LCD_WIDTH_REF - 1U, LCD_HEIGHT_REF - 1U, color);
}

void lcd_ref_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                       uint16_t color)
{
    if ((x0 > x1) || (y0 > y1)) return;
    if (x1 >= LCD_WIDTH_REF)  x1 = LCD_WIDTH_REF - 1U;
    if (y1 >= LCD_HEIGHT_REF) y1 = LCD_HEIGHT_REF - 1U;

    const uint8_t high_byte = (uint8_t)(color >> 8);
    const uint8_t low_byte  = (uint8_t)(color & 0xFFU);

    lcd_ref_set_window(x0, y0, x1, y1);

    const uint32_t pixel_count = (uint32_t)(x1 - x0 + 1U) * (uint32_t)(y1 - y0 + 1U);

    lcd_write_cmd_12(LCD_CMD_RAMWR);
    {
        HPSYS_GPIO_TypeDef *gpio = HPSYS_GPIO;
        const uint32_t clk_mask = (1UL << LCD_CLK);
        const uint32_t d0_mask  = (1UL << LCD_D0);
        const uint32_t d1_mask  = (1UL << LCD_D1);
        const uint32_t d2_mask  = (1UL << LCD_D2);
        const uint32_t d3_mask  = (1UL << LCD_D3);
        const uint32_t data_mask = d0_mask | d1_mask | d2_mask | d3_mask;

        const uint32_t hb_hi_set = ((high_byte & 0x10U) ? d0_mask : 0U)
                                 | ((high_byte & 0x20U) ? d1_mask : 0U)
                                 | ((high_byte & 0x40U) ? d2_mask : 0U)
                                 | ((high_byte & 0x80U) ? d3_mask : 0U);
        const uint32_t hb_hi_clr = data_mask & ~hb_hi_set;
        const uint32_t hb_lo_set = ((high_byte & 0x01U) ? d0_mask : 0U)
                                 | ((high_byte & 0x02U) ? d1_mask : 0U)
                                 | ((high_byte & 0x04U) ? d2_mask : 0U)
                                 | ((high_byte & 0x08U) ? d3_mask : 0U);
        const uint32_t hb_lo_clr = data_mask & ~hb_lo_set;
        const uint32_t lb_hi_set = ((low_byte & 0x10U) ? d0_mask : 0U)
                                 | ((low_byte & 0x20U) ? d1_mask : 0U)
                                 | ((low_byte & 0x40U) ? d2_mask : 0U)
                                 | ((low_byte & 0x80U) ? d3_mask : 0U);
        const uint32_t lb_hi_clr = data_mask & ~lb_hi_set;
        const uint32_t lb_lo_set = ((low_byte & 0x01U) ? d0_mask : 0U)
                                 | ((low_byte & 0x02U) ? d1_mask : 0U)
                                 | ((low_byte & 0x04U) ? d2_mask : 0U)
                                 | ((low_byte & 0x08U) ? d3_mask : 0U);
        const uint32_t lb_lo_clr = data_mask & ~lb_lo_set;

        lcd_push_color_pixels_ram(gpio, pixel_count,
                                  clk_mask, hb_hi_set, hb_hi_clr,
                                  hb_lo_set, hb_lo_clr, lb_hi_set,
                                  lb_hi_clr, lb_lo_set, lb_lo_clr);
    }
    qspi_cmd_end();
}

void lcd_ref_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= LCD_WIDTH_REF) || (y >= LCD_HEIGHT_REF)) return;

    lcd_ref_set_window(x, y, x, y);

    lcd_write_cmd_12(LCD_CMD_RAMWR);
    qspi_send_byte_4wire((uint8_t)(color >> 8));
    qspi_send_byte_4wire((uint8_t)(color & 0xFFU));
    qspi_cmd_end();
}

void lcd_ref_bitblt(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    const uint16_t* rgb565)
{
    if (rgb565 == NULL || w == 0U || h == 0U) return;
    if (x >= LCD_WIDTH_REF || y >= LCD_HEIGHT_REF) return;
    if ((uint32_t)x + w > LCD_WIDTH_REF)  w = LCD_WIDTH_REF - x;
    if ((uint32_t)y + h > LCD_HEIGHT_REF) h = LCD_HEIGHT_REF - y;

    lcd_ref_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

    lcd_write_cmd_12(LCD_CMD_RAMWR);
    {
        HPSYS_GPIO_TypeDef *gpio = HPSYS_GPIO;
        const uint32_t clk_mask = (1UL << LCD_CLK);
        const uint32_t d0_mask  = (1UL << LCD_D0);
        const uint32_t d1_mask  = (1UL << LCD_D1);
        const uint32_t d2_mask  = (1UL << LCD_D2);
        const uint32_t d3_mask  = (1UL << LCD_D3);
        const uint32_t data_mask = d0_mask | d1_mask | d2_mask | d3_mask;

        lcd_push_pixels_ram(gpio, rgb565, (uint32_t)w * (uint32_t)h,
                            clk_mask, d0_mask, d1_mask,
                            d2_mask, d3_mask, data_mask);
    }
    qspi_cmd_end();
}

/** RAM-based buffer fill — runs from RAM to avoid Flash wait states. */
SF32_RAMFUNC __attribute__((noinline))
void lcd_ref_fill_buf(uint16_t* buf, int stride,
                      int w, int h, uint16_t color)
{
    for (int ty = 0; ty < h; ty++) {
        for (int tx = 0; tx < w; tx++) {
            buf[tx] = color;
        }
        buf += stride;
    }
}
