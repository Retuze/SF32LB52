/**
 * @file lcd.c
 * @brief LCD framework — panel singleton, delegates to bus + IC driver.
 */

#include "lcd.h"
#include "board.h"
#include "hal.h"

/* Bus provides these by convention — any bus implementation exports them.
 * QSPI bus: ramwr uses 0x12 framing, push uses direct GPIO registers.
 * SPI bus:  ramwr uses DC pin, push uses SPI single-line. */
extern void lcd_send(const uint16_t *pixels, uint32_t n);
extern void lcd_fill(uint16_t color, uint32_t n);

/* ── Panel state ──────────────────────────────────────────────────────────── */

static struct {
    const lcd_bus_t *bus;
    const lcd_ic_t  *ic;
    uint32_t pin_rst, pin_bl;
} g = { .pin_rst = 0xFFFFFFFF, .pin_bl = 0xFFFFFFFF };

void lcd_set_bus(const lcd_bus_t *b)               { g.bus = b; }
void lcd_set_ic(const lcd_ic_t *i)                 { g.ic  = i; }
void lcd_set_ctrl_pins(uint32_t rst, uint32_t bl) { g.pin_rst = rst; g.pin_bl = bl; }

/* ── Init ──────────────────────────────────────────────────────────────────── */

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
    int r = g.ic->init(g.bus);
    lcd_te_init();
    return r;

    return r;
}

void lcd_sleep(int on)    { if (g.ic->sleep) g.ic->sleep(g.bus, on); }
uint32_t lcd_read_id(void) { return g.ic->read_id ? g.ic->read_id(g.bus) : 0; }

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    g.ic->set_window(g.bus, x0, y0, x1, y1);
}

/* ── Drawing ──────────────────────────────────────────────────────────── */

__attribute__((weak))
void lcd_bitblt(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                const uint16_t *rgb565,
                void (*done)(void *ctx), void *ctx)
{
    // 需要在lcd_bus中实现该接口
}

/* ── Async support helpers (weak defaults — overridden by LCDC bus driver) ─── */

__attribute__((weak))
void lcd_wait_idle(void) {}

__attribute__((weak))
uint32_t lcd_xfer_cycles(void) { return 0; }

__attribute__((weak))
uint32_t lcd_wait_cycles(void) { return 0; }

__attribute__((weak))
void lcd_clear_xfer_cycles(void) {}

/* ── TE frame sync ──────────────────────────────────────────────────────── */

static volatile int s_te_flag = 0;
static volatile int s_te_late = 0;   /* TE fired while xfer still busy */

__attribute__((weak))
int lcd_is_busy(void) { return 0; }

static void te_irq_handler(uint32_t pin, void *arg)
{
    (void)pin; (void)arg;
    s_te_flag = 1;
    if (lcd_is_busy()) s_te_late = 1;
}

int lcd_te_late_count(void)
{
    int n = s_te_late;
    s_te_late = 0;
    return n;
}

__attribute__((weak))
void lcd_te_init(void)
{
    pinMode(LCD_TE, INPUT_PULLUP);
    attachInterrupt(LCD_TE, te_irq_handler, RISING, NULL);
}

__attribute__((weak))
void lcd_te_wait(void)
{
    s_te_flag = 0;
    while (!s_te_flag) { /* spin */ }
    s_te_flag = 0;
}

