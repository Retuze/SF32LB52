/**
 * @file lcd.h
 * @brief LCD framework — three-layer architecture.
 *
 *   Panel (lcd.c)         — RST/BL pins, init orchestration, drawing API
 *   Bus   (lcd_bus_*.c)   — transport protocol (SPI, QSPI, MIPI)
 *   IC    (lcd_ic_*.c)    — panel-specific commands (CO5300, ST7789, ...)
 *
 * Bus interface uses a `framing` byte so the caller controls QSPI mode
 * (0x02 single, 0x12 single-cmd+quad-data, 0x32 quad-cmd+quad-data).
 * SPI implementations ignore the framing byte and use DC pin instead.
 *
 * Fast pixel-push (lcd_*_push_pixels / lcd_*_push_color) are plain C
 * functions, not vtable pointers — called directly after cmd_write(RAMWR).
 */

#pragma once
#ifndef LCD_H
#define LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Bus ──────────────────────────────────────────────────────────────── */

/**
 * Framing byte — bus-specific.  QSPI uses 0x02/0x12/0x32; SPI ignores it
 * (DC pin handles command-vs-data).  Each bus header defines its own
 * constants for the framing values it supports.
 */
typedef struct lcd_bus {
    void (*init)(void);
    void (*begin)(void);
    void (*end)(void);

    void (*cmd_write)(uint8_t cmd, const uint8_t *param, uint32_t param_len);
    void (*cmd_read) (uint8_t cmd, uint8_t *data, uint32_t data_len);
} lcd_bus_t;

/* ── IC ───────────────────────────────────────────────────────────────── */

typedef struct lcd_ic {
    const char *name;
    int  (*init)(const lcd_bus_t *bus);
    void (*set_window)(const lcd_bus_t *bus,
                       uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void (*sleep)(const lcd_bus_t *bus, int on);
    uint32_t (*read_id)(const lcd_bus_t *bus);
} lcd_ic_t;

/* ── Registration ────────────────────────────────────────────────────── */

void lcd_set_bus(const lcd_bus_t *bus);
void lcd_set_ic(const lcd_ic_t *ic);
void lcd_set_pins(uint32_t rst, uint32_t bl);
void lcd_set_geometry(uint16_t w, uint16_t h);

/* ── Public API ──────────────────────────────────────────────────────── */

int      lcd_init(void);
void     lcd_sleep(int on);
uint32_t lcd_read_id(void);

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void lcd_fill_color(uint16_t color);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_bitblt(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565);

/* Async bitblt — weak defaults in lcd.c, overridden by LCDC bus driver */
void lcd_bitblt_async(uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h,
                      const uint16_t *rgb565,
                      void (*done)(void *ctx), void *ctx);
void lcd_wait_idle(void);
uint32_t lcd_xfer_cycles(void);
void     lcd_clear_xfer_cycles(void);

/* ── Available instances ─────────────────────────────────────────────── */

extern const lcd_bus_t lcd_bus_qspi;
extern const lcd_bus_t lcd_bus_qspi_lcdc;
extern const lcd_ic_t  lcd_ic_co5300;

#ifdef __cplusplus
}
#endif

#endif /* LCD_H */
