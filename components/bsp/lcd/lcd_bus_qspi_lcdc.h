/**
 * @file lcd_bus_qspi_lcdc.h
 * @brief LCDC-based QSPI bus — async API for LithoUI PFB pipeline.
 *
 * Include this from display adapter code that needs hardware-accelerated
 * QSPI with async (DMA + interrupt) completion.
 */

#pragma once
#ifndef LCD_BUS_QSPI_LCDC_H
#define LCD_BUS_QSPI_LCDC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sync bitblt (blocking) */
void lcd_bus_qspi_lcdc_bitblt(uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h,
                               const uint16_t *rgb565);

/* Async bitblt — returns immediately, calls done(ctx) from ISR on completion */
void lcd_bus_qspi_lcdc_bitblt_async(uint16_t x, uint16_t y,
                                     uint16_t w, uint16_t h,
                                     const uint16_t *rgb565,
                                     void (*done)(void *ctx), void *ctx);

/* Busy-wait until async xfer completes */
void lcd_bus_qspi_lcdc_wait_idle(void);

/* Transfer cycle counters (DWT CYCCNT ticks) */
uint32_t lcd_bus_qspi_lcdc_xfer_cycles(void);
void     lcd_bus_qspi_lcdc_clear_xfer_cycles(void);

/* The bus vtable instance */
extern const struct lcd_bus lcd_bus_qspi_lcdc;

#ifdef __cplusplus
}
#endif

#endif /* LCD_BUS_QSPI_LCDC_H */
