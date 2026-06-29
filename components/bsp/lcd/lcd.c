/**
 * @file lcd.c
 * @brief LCD framework — panel singleton, delegates to bus + IC driver.
 */

#include "lcd.h"
#include "hal.h"

/* Bus provides these by convention — any bus implementation exports them.
 * QSPI bus: ramwr uses 0x12 framing, push uses direct GPIO registers.
 * SPI bus:  ramwr uses DC pin, push uses SPI single-line. */
/* Bus provides pixel output by convention — one function per shape. */
extern void lcd_send(const uint16_t *pixels, uint32_t n);
extern void lcd_fill(uint16_t color, uint32_t n);

/* ── Panel state ─────────────────────────────────────────────────────── */

static struct {
    const lcd_bus_t *bus;
    const lcd_ic_t  *ic;
    uint16_t width, height;
    uint32_t pin_rst, pin_bl;
} g = { .width = 390, .height = 450,
        .pin_rst = 0xFFFFFFFF, .pin_bl = 0xFFFFFFFF };

void lcd_set_bus(const lcd_bus_t *b)      { g.bus = b; }
void lcd_set_ic(const lcd_ic_t *i)        { g.ic  = i; }
void lcd_set_pins(uint32_t rst, uint32_t bl) { g.pin_rst = rst; g.pin_bl = bl; }
void lcd_set_geometry(uint16_t w, uint16_t h) { g.width = w; g.height = h; }

/* ── Init ─────────────────────────────────────────────────────────────── */

int lcd_init(void)
{
    if (!g.bus || !g.ic) return -1;

    g.bus->init();

    if (g.pin_rst != 0xFFFFFFFF) {
        pinMode(g.pin_rst, OUTPUT);
        digitalWrite(g.pin_rst, HIGH); delay(10);
        digitalWrite(g.pin_rst, LOW);  delay(10);
        digitalWrite(g.pin_rst, HIGH); delay(50);
    }
    if (g.pin_bl != 0xFFFFFFFF) {
        pinMode(g.pin_bl, OUTPUT);
        digitalWrite(g.pin_bl, HIGH);
    }
    return g.ic->init(g.bus);
}

void lcd_sleep(int on)    { if (g.ic->sleep) g.ic->sleep(g.bus, on); }
uint32_t lcd_read_id(void) { return g.ic->read_id ? g.ic->read_id(g.bus) : 0; }

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    g.ic->set_window(g.bus, x0, y0, x1, y1);
}

/* ── Drawing ──────────────────────────────────────────────────────────── */

void lcd_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if (x0 > x1 || y0 > y1) return;
    if (x1 >= g.width)  x1 = g.width - 1U;
    if (y1 >= g.height) y1 = g.height - 1U;

    lcd_set_window(x0, y0, x1, y1);
    lcd_fill(color, (uint32_t)(x1 - x0 + 1U) * (uint32_t)(y1 - y0 + 1U));
}

void lcd_fill_color(uint16_t color)
{
    lcd_fill_rect(0, 0, g.width - 1U, g.height - 1U, color);
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= g.width || y >= g.height) return;
    lcd_set_window(x, y, x, y);
    lcd_fill(color, 1);
}

void lcd_bitblt(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565)
{
    if (!rgb565 || !w || !h) return;
    if (x >= g.width || y >= g.height) return;
    if ((uint32_t)x + w > g.width)  w = g.width - x;
    if ((uint32_t)y + h > g.height) h = g.height - y;

    lcd_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    lcd_send(rgb565, (uint32_t)w * (uint32_t)h);
}

/* ── Async bitblt (weak defaults — overridden by LCDC bus driver) ────────── */

__attribute__((weak))
void lcd_bitblt_async(uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h,
                      const uint16_t *rgb565,
                      void (*done)(void *ctx), void *ctx)
{
    lcd_bitblt(x, y, w, h, rgb565);
    if (done) done(ctx);
}

__attribute__((weak))
void lcd_wait_idle(void) {}

__attribute__((weak))
uint32_t lcd_xfer_cycles(void) { return 0; }

__attribute__((weak))
void lcd_clear_xfer_cycles(void) {}

