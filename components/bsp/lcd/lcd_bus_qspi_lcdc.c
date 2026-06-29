/**
 * @file lcd_bus_qspi_lcdc.c
 * @brief QSPI transport using LCDC hardware peripheral (DMA-driven, async).
 *
 * Implements lcd_bus_t so the existing IC driver (lcd_ic_co5300.c) works
 * unchanged.  Provides lcd_send / lcd_fill / lcd_bitblt_async (strong) that
 * override the weak defaults in lcd.c when this driver is linked.
 *
 * Only ONE bus driver (bit-bang or LCDC) must be compiled into the firmware.
 * Select via CMakeLists.txt.
 *
 * Compared to bit-bang (lcd_bus_qspi.c):
 *   - cmd/cmd_read use LCDC single-write registers
 *   - Pixel push uses LCDC DMA → up to 80 MHz QSPI
 *   - Async xfer frees CPU to draw the next tile while LCDC transmits
 *
 * References:
 *   - Remote commit eec78ac7 (lcd_lcdc_co5300.c)
 *   - SIFLI SDK bf0_hal_lcdc.c
 */

#include "lcd.h"
#include "SF32LB52.h"
#include "board.h"
#include "hal.h"
#include "bf0_hal_lcdc.h"

#include <stdint.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * LCDC framing bytes (QSPI 4-line with DCX)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LCDC_SPI_WRITE_CMD     0x02U
#define LCDC_SPI_WRITE_RAM_CMD 0x32U
#define LCDC_SPI_READ_CMD      0x03U

#ifndef LCDC_QSPI_FREQ_HZ
#define LCDC_QSPI_FREQ_HZ      80000000U
#endif

#define LCDC1_IRQN             63U

/* ═══════════════════════════════════════════════════════════════════════════
 * Static LCDC state
 * ═══════════════════════════════════════════════════════════════════════════ */

static LCDC_HandleTypeDef s_lcdc;
static volatile int s_busy;
static void (*s_done)(void *ctx);
static void *s_done_ctx;
static uint32_t s_xfer_start;
static uint32_t s_xfer_cycles;

/* Fill buffer for lcd_fill (one row, reused) */
static uint16_t s_fill_row[LCD_WIDTH];

/* ═══════════════════════════════════════════════════════════════════════════
 * D-Cache maintenance (required before LCDC DMA reads from SRAM)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void dcache_clean_by_addr(const void *addr, uint32_t size)
{
    uintptr_t start = (uintptr_t)addr & ~(uintptr_t)31U;
    uintptr_t end   = ((uintptr_t)addr + size + 31U) & ~(uintptr_t)31U;
    volatile uint32_t *dccimvac = (volatile uint32_t *)0xE000EF70UL;

    for (uintptr_t p = start; p < end; p += 32U) {
        *dccimvac = (uint32_t)p;
    }
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Pinmux — adapted from remote lcd_lcdc_co5300.c
 * ═══════════════════════════════════════════════════════════════════════════ */

static void lcdc_pinmux_pa(uint32_t pin, uint32_t fsel, uint32_t flags)
{
    enum { PA_PAD_OFFSET = 13U };

    sf32lb52_pinmux_enable_clock();
    uint32_t cfg = flags & (SF32_PINMUX_PULL_ENABLE | SF32_PINMUX_PULL_SELECT_UP |
                            SF32_PINMUX_INPUT_ENABLE | SF32_PINMUX_INPUT_SCHMITT |
                            SF32_PINMUX_SLEW_SLOW | SF32_PINMUX_DRIVE_Msk);
    cfg |= (fsel << SF32_PINMUX_FSEL_Pos) & SF32_PINMUX_FSEL_Msk;
    HPSYS_PINMUX->PAD[PA_PAD_OFFSET + pin].R = cfg;
}

static void lcdc_pinmux(void)
{
    const uint32_t flags = SF32_PINMUX_PULL_NONE | SF32_PINMUX_DRIVE_3
                         | SF32_PINMUX_INPUT_ENABLE;
    lcdc_pinmux_pa(LCD_TE,  1U, flags);
    lcdc_pinmux_pa(LCD_CS,  1U, flags);
    lcdc_pinmux_pa(LCD_CLK, 1U, flags);
    lcdc_pinmux_pa(LCD_D0,  1U, flags);
    lcdc_pinmux_pa(LCD_D1,  1U, flags);
    lcdc_pinmux_pa(LCD_D2,  1U, flags);
    lcdc_pinmux_pa(LCD_D3,  1U, flags);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LCDC register I/O helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static void lcdc_write_reg(uint8_t reg, const uint8_t *data, uint32_t len)
{
    uint32_t cmd = (reg == 0x2CU)
                   ? ((LCDC_SPI_WRITE_RAM_CMD << 24) | ((uint32_t)reg << 8))
                   : ((LCDC_SPI_WRITE_CMD      << 24) | ((uint32_t)reg << 8));
    HAL_LCDC_WriteU32Reg(&s_lcdc, cmd, (uint8_t *)data, len);
}

static uint32_t lcdc_read_reg(uint8_t reg, uint8_t len)
{
    uint32_t data = 0U;
    HAL_LCDC_SetFreq(&s_lcdc, 500000U);
    HAL_LCDC_ReadU32Reg(&s_lcdc,
                        ((LCDC_SPI_READ_CMD << 24) | ((uint32_t)reg << 8)),
                        (uint8_t *)&data, len);
    HAL_LCDC_SetFreq(&s_lcdc, LCDC_QSPI_FREQ_HZ);
    return data;
}

static void lcdc_xfer_done(LCDC_HandleTypeDef *lcdc)
{
    (void)lcdc;
    s_xfer_cycles += DWT_CYCCNT - s_xfer_start;
    s_busy = 0;
    if (s_done != NULL) {
        void (*cb)(void *) = s_done;
        void *ctx = s_done_ctx;
        s_done = NULL;
        s_done_ctx = NULL;
        cb(ctx);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lcd_bus_t implementation (cmd/register I/O via LCDC single-write)
 * CS is auto-managed by LCDC hardware.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void lcdc_init(void)
{
    memset(&s_lcdc, 0, sizeof s_lcdc);
    lcdc_pinmux();

    s_lcdc.Instance = LCDC1;
    s_lcdc.Init.lcd_itf = LCDC_INTF_SPI_DCX_4DATA;
    s_lcdc.Init.freq = LCDC_QSPI_FREQ_HZ;
    s_lcdc.Init.color_mode = LCDC_PIXEL_FORMAT_RGB565;
    s_lcdc.Init.cfg.spi.dummy_clock = 0;
    s_lcdc.Init.cfg.spi.syn_mode = HAL_LCDC_SYNC_DISABLE;
    s_lcdc.Init.cfg.spi.cs_polarity = 0;
    s_lcdc.Init.cfg.spi.clk_polarity = 0;
    s_lcdc.Init.cfg.spi.clk_phase = 0;
    s_lcdc.Init.cfg.spi.bytes_gap_us = 0;
    s_lcdc.Init.cfg.spi.vsyn_polarity = 1;
    s_lcdc.Init.cfg.spi.vsyn_delay_us = 0;
    s_lcdc.Init.cfg.spi.hsyn_num = 0;
    s_lcdc.XferCpltCallback = lcdc_xfer_done;

    HAL_LCDC_Init(&s_lcdc);

    /* Enable LCDC1 IRQ */
    volatile uint32_t *iser = (volatile uint32_t *)0xE000E100UL;
    iser[LCDC1_IRQN >> 5] = 1UL << (LCDC1_IRQN & 31U);
}

static void lcdc_begin(void)  { /* CS auto by LCDC */ }
static void lcdc_end(void)    { /* CS auto by LCDC */ }

static void lcdc_cmd_write(uint8_t cmd, const uint8_t *param, uint32_t param_len)
{
    lcdc_write_reg(cmd, param, param_len);
}

static void lcdc_cmd_read(uint8_t cmd, uint8_t *data, uint32_t data_len)
{
    if (!data || !data_len) return;
    for (uint32_t i = 0U; i < data_len; i++) {
        uint32_t v = lcdc_read_reg(cmd, 1);
        data[i] = (uint8_t)(v & 0xFFU);
    }
}

const lcd_bus_t lcd_bus_qspi_lcdc = {
    .init      = lcdc_init,
    .begin     = lcdc_begin,
    .end       = lcdc_end,
    .cmd_write = lcdc_cmd_write,
    .cmd_read  = lcdc_cmd_read,
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Fast pixel output (strong — replaces bit-bang lcd_send / lcd_fill)
 * Called by lcd.c's lcd_bitblt / lcd_fill_rect.
 * ═══════════════════════════════════════════════════════════════════════════ */

RAMFUNC __attribute__((noinline))
void lcd_send(const uint16_t *pixels, uint32_t n)
{
    if (!pixels || !n) return;

    while (s_busy) {}

    uint16_t w = (uint16_t)n;
    uint16_t h = 1U;

    HAL_LCDC_SetBgColor(&s_lcdc, 0, 0, 0);
    HAL_LCDC_LayerEnable(&s_lcdc, HAL_LCDC_LAYER_DEFAULT);
    HAL_LCDC_LayerEnableAlpha(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, 255);
    dcache_clean_by_addr(pixels, n * sizeof(uint16_t));
    HAL_LCDC_LayerSetFormat(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB565);
    HAL_LCDC_LayerSetData(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, (uint8_t *)pixels,
                           0, 0, (uint16_t)(w - 1U), (uint16_t)(h - 1U));

    uint32_t cmd = ((uint32_t)LCDC_SPI_WRITE_RAM_CMD << 24) | (0x2CU << 8);
    s_busy = 1;
    s_xfer_start = DWT_CYCCNT;
    HAL_LCDC_SendLayerData2Reg(&s_lcdc, cmd, 4);
    while (s_busy) {}
}

RAMFUNC __attribute__((noinline))
void lcd_fill(uint16_t color, uint32_t n)
{
    if (!n) return;

    uint32_t row_px = ((uint32_t)n < (uint32_t)LCD_WIDTH) ? n : (uint32_t)LCD_WIDTH;
    for (uint32_t i = 0U; i < row_px; i++) {
        s_fill_row[i] = color;
    }

    while (n > 0U) {
        uint32_t chunk = (n > row_px) ? row_px : n;
        lcd_send(s_fill_row, chunk);
        n -= chunk;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Async bitblt API (strong — overrides weak defaults in lcd.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

void lcd_bitblt_async(uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h,
                      const uint16_t *rgb565,
                      void (*done)(void *ctx), void *ctx)
{
    if (rgb565 == NULL || w == 0U || h == 0U) {
        if (done != NULL) done(ctx);
        return;
    }
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        if (done != NULL) done(ctx);
        return;
    }
    if ((uint32_t)x + w > LCD_WIDTH)  w = (uint16_t)(LCD_WIDTH  - x);
    if ((uint32_t)y + h > LCD_HEIGHT) h = (uint16_t)(LCD_HEIGHT - y);

    lcd_wait_idle();

    HAL_LCDC_SetBgColor(&s_lcdc, 0, 0, 0);
    HAL_LCDC_LayerEnable(&s_lcdc, HAL_LCDC_LAYER_DEFAULT);
    HAL_LCDC_LayerEnableAlpha(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, 255);
    dcache_clean_by_addr(rgb565, (uint32_t)w * (uint32_t)h * sizeof(uint16_t));
    HAL_LCDC_LayerSetFormat(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB565);
    HAL_LCDC_LayerSetData(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, (uint8_t *)rgb565,
                           x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

    uint32_t cmd = ((uint32_t)LCDC_SPI_WRITE_RAM_CMD << 24) | (0x2CU << 8);
    s_done = done;
    s_done_ctx = ctx;
    s_busy = 1;
    s_xfer_start = DWT_CYCCNT;

    if (HAL_LCDC_SendLayerData2Reg_IT(&s_lcdc, cmd, 4) != HAL_OK) {
        s_busy = 0;
        s_done = NULL;
        s_done_ctx = NULL;
        if (done != NULL) done(ctx);
    }
}

void lcd_wait_idle(void)
{
    while (s_busy) {}
}

uint32_t lcd_xfer_cycles(void)
{
    return s_xfer_cycles;
}

void lcd_clear_xfer_cycles(void)
{
    s_xfer_cycles = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LCDC1 interrupt handler
 * ═══════════════════════════════════════════════════════════════════════════ */

void LCDC1_IRQHandler(void)
{
    HAL_LCDC_IRQHandler(&s_lcdc);
}
