/**
 * @file lcd_ref.h
 * @brief Direct port of the reference lcd_test project's LCD driver.
 *
 * Uses hardcoded GPIO pin-macros from board.h, matching the original
 * SDK bit-bang QSPI implementation exactly.
 */
#pragma once
#ifndef LCD_REF_H
#define LCD_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Panel dimensions (from board.h, shared with the ported lcd_ref.c). */
#define LCD_WIDTH_REF  390U
#define LCD_HEIGHT_REF 450U

void lcd_ref_reset(void);
void lcd_ref_read_id(uint8_t id[3]);
int  lcd_ref_init(void);

void lcd_ref_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_ref_fill_color(uint16_t color);
void lcd_ref_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                       uint16_t color);
void lcd_ref_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/** Push arbitrary RGB565 pixel data (from buffer) at (x,y) with given w,h.
 *  Uses fast HPSYS_GPIO register writes — same speed as fill_rect. */
void lcd_ref_bitblt(uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h,
                    const uint16_t* rgb565);

/** RAM-based fast buffer fill (RGB565 constant color). */
void lcd_ref_fill_buf(uint16_t* buf, int stride,
                      int w, int h, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* LCD_REF_H */
