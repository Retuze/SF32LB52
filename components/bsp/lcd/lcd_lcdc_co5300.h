#pragma once
#ifndef LCD_LCDC_CO5300_H
#define LCD_LCDC_CO5300_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  lcd_lcdc_co5300_init(void);
void lcdc_activate_pixel(void);  /* pinmux + LCDC init only, no panel reset */
void lcd_lcdc_co5300_bitblt(uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h,
                            const uint16_t *rgb565);
void lcd_lcdc_co5300_bitblt_async(uint16_t x, uint16_t y,
                                  uint16_t w, uint16_t h,
                                  const uint16_t *rgb565,
                                  void (*done)(void *ctx), void *ctx);
void lcd_lcdc_co5300_wait_idle(void);
uint32_t lcd_lcdc_co5300_xfer_cycles(void);
void lcd_lcdc_co5300_clear_xfer_cycles(void);

#ifdef __cplusplus
}
#endif

#endif
