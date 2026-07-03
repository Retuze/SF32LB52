/**
 * @file lcd_bus_qspi_lcdc.c
 * @brief QSPI bus driver — hardware LCDC DMA.
 *
 * Overrides the weak lcd_* symbols in lcd.c with LCDC hardware-accelerated
 * paths.  Complements lcd_bus_qspi_gpio.c (bit-bang) which handles panel init.
 */

#include "lcd.h"
#include "board.h"
#include "hal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REG_WRITE_RAM          0x2CU
#define REG_CASET              0x2AU
#define REG_RASET              0x2BU

#define LCDC_SPI_WRITE_CMD     0x02U
#define LCDC_SPI_WRITE_RAM_CMD 0x32U

#define LCDC1_IRQN             63U
#ifndef LCDC_QSPI_FREQ_HZ
#define LCDC_QSPI_FREQ_HZ      50000000U
#endif

/* ── Static state ──────────────────────────────────────────────────────── */

static LCDC_HandleTypeDef s_lcdc;
static volatile int s_busy;
static void (*s_done)(void *ctx);
static void *s_done_ctx;
static uint32_t s_xfer_start;
static uint32_t s_xfer_cycles;

/* ── D-Cache maintenance ───────────────────────────────────────────────── */

static void dcache_clean_by_addr(const void *addr, uint32_t size)
{
    uintptr_t start = (uintptr_t)addr & ~(uintptr_t)31U;
    uintptr_t end = ((uintptr_t)addr + size + 31U) & ~(uintptr_t)31U;
    volatile uint32_t *dccimvac = (volatile uint32_t *)0xE000EF70UL;

    for (uintptr_t p = start; p < end; p += 32U) {
        *dccimvac = (uint32_t)p;
    }
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
}

/* ── Pinmux ────────────────────────────────────────────────────────────── */

static void lcdc_pinmux_inner(void)
{
    pinmux_clk_enable();

    const uint32_t flags = PINMUX_PULL_NONE | PINMUX_DRIVE_3 | PINMUX_INPUT_ENABLE;
    uint32_t cfg = flags & (PINMUX_PULL_ENABLE | PINMUX_PULL_UP_SEL |
                             PINMUX_INPUT_ENABLE | PINMUX_INPUT_SCHMITT |
                             PINMUX_SLEW_SLOW | PINMUX_DRIVE_Msk);
    cfg |= (1U << PINMUX_FSEL_Pos) & PINMUX_FSEL_Msk;  /* fsel=1 → LCDC */

#define LCDC_WRITE_PAD(pin) do {                              \
    HPSYS_PINMUX->PAD[PA_PAD_OFFSET + (pin)].R = cfg;        \
    __asm volatile("dsb" ::: "memory");                       \
    (void)HPSYS_PINMUX->PAD[PA_PAD_OFFSET + (pin)].R;        \
} while(0)

    /* LCD_TE is managed by lcd.c as GPIO EXTI — skip here */
    LCDC_WRITE_PAD(LCD_CS);
    LCDC_WRITE_PAD(LCD_CLK);
    LCDC_WRITE_PAD(LCD_D0);
    LCDC_WRITE_PAD(LCD_D1);
    LCDC_WRITE_PAD(LCD_D2);
    LCDC_WRITE_PAD(LCD_D3);

#undef LCDC_WRITE_PAD
}

/* ── Register I/O ──────────────────────────────────────────────────────── */

static void lcdc_write_reg(uint16_t reg, const uint8_t *data, uint32_t len)
{
    uint32_t cmd = (reg == REG_WRITE_RAM) ?
                   ((LCDC_SPI_WRITE_RAM_CMD << 24) | ((uint32_t)reg << 8)) :
                   ((LCDC_SPI_WRITE_CMD << 24) | ((uint32_t)reg << 8));
    HAL_LCDC_WriteU32Reg(&s_lcdc, cmd, (uint8_t *)data, len);
}

static void lcdc_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t p[4];
    HAL_LCDC_SetROIArea(&s_lcdc, x0, y0, x1, y1);

    p[0] = (uint8_t)(x0 >> 8); p[1] = (uint8_t)x0;
    p[2] = (uint8_t)(x1 >> 8); p[3] = (uint8_t)x1;
    lcdc_write_reg(REG_CASET, p, sizeof p);

    p[0] = (uint8_t)(y0 >> 8); p[1] = (uint8_t)y0;
    p[2] = (uint8_t)(y1 >> 8); p[3] = (uint8_t)y1;
    lcdc_write_reg(REG_RASET, p, sizeof p);
}

/* ── DMA ISR ───────────────────────────────────────────────────────────── */

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

void LCDC1_IRQHandler(void)
{
    HAL_LCDC_IRQHandler(&s_lcdc);
}

/* ── Bus post-init (override lcd.c weak) ───────────────────────────────── */

void lcd_bus_init(void)
{
    memset(&s_lcdc, 0, sizeof s_lcdc);
    lcdc_pinmux_inner();

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
    nvic_enable_irq(LCDC1_IRQN);

    printf("[lcdc] pixel DMA activated\r\n");
}

/* ── Async bitblt (override lcd.c weak) ────────────────────────────────── */

void lcd_wait_idle(void)
{
    while (s_busy) {
    }
}

int lcd_is_busy(void)
{
    return s_busy;
}

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
    if ((uint32_t)x + w > LCD_WIDTH)  w = (uint16_t)(LCD_WIDTH - x);
    if ((uint32_t)y + h > LCD_HEIGHT) h = (uint16_t)(LCD_HEIGHT - y);

    lcd_wait_idle();

    HAL_LCDC_SetBgColor(&s_lcdc, 0, 0, 0);
    lcdc_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    HAL_LCDC_LayerEnable(&s_lcdc, HAL_LCDC_LAYER_DEFAULT);
    HAL_LCDC_LayerEnableAlpha(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, 255);
    dcache_clean_by_addr(rgb565, (uint32_t)w * (uint32_t)h * sizeof(uint16_t));
    HAL_LCDC_LayerSetFormat(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB565);
    HAL_LCDC_LayerSetDataExt(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, (uint8_t *)rgb565,
                             x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), w);

    uint32_t cmd = ((uint32_t)LCDC_SPI_WRITE_RAM_CMD << 24) | ((uint32_t)REG_WRITE_RAM << 8);
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

uint32_t lcd_xfer_cycles(void)
{
    return s_xfer_cycles;
}

void lcd_clear_xfer_cycles(void)
{
    s_xfer_cycles = 0;
}
