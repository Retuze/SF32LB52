#include "lcd_lcdc_co5300.h"

#include "board.h"
#include "hal.h"

#include <stdint.h>
#include <string.h>

int printf(const char *fmt, ...);

#define REG_LCD_ID             0x04U
#define REG_SLEEP_OUT          0x11U
#define REG_DISPLAY_ON         0x29U
#define REG_WRITE_RAM          0x2CU
#define REG_CASET              0x2AU
#define REG_RASET              0x2BU
#define REG_TEARING_EFFECT_ON  0x35U
#define REG_COLOR_MODE         0x3AU
#define REG_WBRIGHT            0x51U
#define REG_WRITE_CTRL_DISPLAY 0x53U
#define REG_WRHBMDISBV         0x63U
#define REG_SET_SPI_MODE       0xC4U
#define REG_PASSWD1            0xF4U
#define REG_PASSWD2            0xF5U
#define REG_CMD_PAGE_SWITCH    0xFEU

#define LCDC_SPI_WRITE_CMD     0x02U
#define LCDC_SPI_WRITE_RAM_CMD 0x32U
#define LCDC_SPI_READ_CMD      0x03U

#define LCDC1_IRQN             63U
#ifndef LCDC_QSPI_FREQ_HZ
#define LCDC_QSPI_FREQ_HZ      80000000U
#endif

static LCDC_HandleTypeDef s_lcdc;
static volatile int s_busy;
static void (*s_done)(void *ctx);
static void *s_done_ctx;
static uint32_t s_xfer_start;
static uint32_t s_xfer_cycles;
static uint8_t s_clear_buf[LCD_WIDTH * 50 * 2];

static void lcdc_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

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

static void nvic_enable_irq(uint32_t irqn)
{
    volatile uint32_t *iser = (volatile uint32_t *)0xE000E100UL;
    iser[irqn >> 5] = 1UL << (irqn & 31U);
}

static void lcdc_pinmux_pa(uint32_t pin, uint32_t fsel, uint32_t flags)
{
    enum { PA_PAD_OFFSET = 13U };
    uint32_t cfg = flags & (SF32_PINMUX_PULL_ENABLE | SF32_PINMUX_PULL_SELECT_UP |
                            SF32_PINMUX_INPUT_ENABLE | SF32_PINMUX_INPUT_SCHMITT |
                            SF32_PINMUX_SLEW_SLOW | SF32_PINMUX_DRIVE_Msk);

    sf32lb52_pinmux_enable_clock();
    cfg |= (fsel << SF32_PINMUX_FSEL_Pos) & SF32_PINMUX_FSEL_Msk;
    HPSYS_PINMUX->PAD[PA_PAD_OFFSET + pin].R = cfg;
}

static void lcdc_pinmux(void)
{
    const uint32_t flags = SF32_PINMUX_PULL_NONE | SF32_PINMUX_DRIVE_3 | SF32_PINMUX_INPUT_ENABLE;
    lcdc_pinmux_pa(LCD_TE,  1U, flags);
    lcdc_pinmux_pa(LCD_CS,  1U, flags);
    lcdc_pinmux_pa(LCD_CLK, 1U, flags);
    lcdc_pinmux_pa(LCD_D0,  1U, flags);
    lcdc_pinmux_pa(LCD_D1,  1U, flags);
    lcdc_pinmux_pa(LCD_D2,  1U, flags);
    lcdc_pinmux_pa(LCD_D3,  1U, flags);
}

static void lcdc_write_reg(uint16_t reg, const uint8_t *data, uint32_t len)
{
    uint32_t cmd = (reg == REG_WRITE_RAM) ?
                   ((LCDC_SPI_WRITE_RAM_CMD << 24) | ((uint32_t)reg << 8)) :
                   ((LCDC_SPI_WRITE_CMD << 24) | ((uint32_t)reg << 8));
    HAL_LCDC_WriteU32Reg(&s_lcdc, cmd, (uint8_t *)data, len);
}

static uint32_t lcdc_read_reg(uint16_t reg, uint8_t len)
{
    uint32_t data = 0U;
    HAL_LCDC_SetFreq(&s_lcdc, 500000U);
    HAL_LCDC_ReadU32Reg(&s_lcdc,
                        ((LCDC_SPI_READ_CMD << 24) | ((uint32_t)reg << 8)),
                        (uint8_t *)&data, len);
    HAL_LCDC_SetFreq(&s_lcdc, LCDC_QSPI_FREQ_HZ);
    return data;
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

static void lcdc_clear_black_raw(void)
{
    uint32_t cmd = ((uint32_t)LCDC_SPI_WRITE_CMD << 24) | ((uint32_t)REG_WRITE_RAM << 8);
    memset(s_clear_buf, 0, sizeof s_clear_buf);

    for (uint16_t y = 0; y < LCD_HEIGHT; y = (uint16_t)(y + 50U)) {
        uint16_t h = (LCD_HEIGHT - y > 50U) ? 50U : (uint16_t)(LCD_HEIGHT - y);
        lcdc_set_window(0, y, LCD_WIDTH - 1U, (uint16_t)(y + h - 1U));
        HAL_LCDC_WriteU32Reg(&s_lcdc, cmd, s_clear_buf,
                             (uint32_t)LCD_WIDTH * h * sizeof(uint16_t));
    }
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

void LCDC1_IRQHandler(void)
{
    HAL_LCDC_IRQHandler(&s_lcdc);
}

int lcd_lcdc_co5300_init(void)
{
    memset(&s_lcdc, 0, sizeof s_lcdc);
    lcdc_pinmux();

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

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

    pinMode(LCD_RST, OUTPUT);
    digitalWrite(LCD_RST, HIGH);
    delay(10);
    digitalWrite(LCD_RST, LOW);
    delay(10);
    digitalWrite(LCD_RST, HIGH);
    delay(50);

    uint32_t id = lcdc_read_reg(REG_LCD_ID, 3);
    printf("LCD ID(LCDC): %06lx\n", (unsigned long)id);

    uint8_t p[4];
    p[0] = 0x20U; lcdc_write_reg(REG_CMD_PAGE_SWITCH, p, 1);
    p[0] = 0x5AU; lcdc_write_reg(REG_PASSWD1, p, 1);
    p[0] = 0x59U; lcdc_write_reg(REG_PASSWD2, p, 1);
    p[0] = 0x20U; lcdc_write_reg(REG_CMD_PAGE_SWITCH, p, 1);
    p[0] = 0xA5U; lcdc_write_reg(REG_PASSWD1, p, 1);
    p[0] = 0xA5U; lcdc_write_reg(REG_PASSWD2, p, 1);
    p[0] = 0x00U; lcdc_write_reg(REG_CMD_PAGE_SWITCH, p, 1);
    p[0] = 0x80U; lcdc_write_reg(REG_SET_SPI_MODE, p, 1);
    p[0] = 0x55U; lcdc_write_reg(REG_COLOR_MODE, p, 1);
    p[0] = 0x00U; lcdc_write_reg(REG_TEARING_EFFECT_ON, p, 1);
    p[0] = 0x20U; lcdc_write_reg(REG_WRITE_CTRL_DISPLAY, p, 1);
    p[0] = 0x80U; lcdc_write_reg(REG_WBRIGHT, p, 1);
    p[0] = 0xFFU; lcdc_write_reg(REG_WRHBMDISBV, p, 1);

    lcdc_set_window(0, 0, LCD_WIDTH - 1U, LCD_HEIGHT - 1U);
    lcdc_write_reg(REG_SLEEP_OUT, NULL, 0);
    delay(120);
    lcdc_write_reg(REG_DISPLAY_ON, NULL, 0);
    delay(20);

    printf("LCD init done (LCDC) SPI_IF_CONF=0x%08lX freq=%lu\n",
           (unsigned long)s_lcdc.Instance->SPI_IF_CONF,
           (unsigned long)LCDC_QSPI_FREQ_HZ);
    lcdc_clear_black_raw();
    return 0;
}

void lcd_lcdc_co5300_wait_idle(void)
{
    while (s_busy) {
    }
}

void lcd_lcdc_co5300_bitblt_async(uint16_t x, uint16_t y,
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

    lcd_lcdc_co5300_wait_idle();

    HAL_LCDC_SetBgColor(&s_lcdc, 0, 0, 0);
    lcdc_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    HAL_LCDC_LayerEnable(&s_lcdc, HAL_LCDC_LAYER_DEFAULT);
    HAL_LCDC_LayerEnableAlpha(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, 255);
    dcache_clean_by_addr(rgb565, (uint32_t)w * (uint32_t)h * sizeof(uint16_t));
    HAL_LCDC_LayerSetFormat(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB565);
    HAL_LCDC_LayerSetData(&s_lcdc, HAL_LCDC_LAYER_DEFAULT, (uint8_t *)rgb565,
                           x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

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

void lcd_lcdc_co5300_bitblt(uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h,
                            const uint16_t *rgb565)
{
    lcd_lcdc_co5300_bitblt_async(x, y, w, h, rgb565, NULL, NULL);
    lcd_lcdc_co5300_wait_idle();
}

uint32_t lcd_lcdc_co5300_xfer_cycles(void)
{
    return s_xfer_cycles;
}

void lcd_lcdc_co5300_clear_xfer_cycles(void)
{
    s_xfer_cycles = 0;
}
