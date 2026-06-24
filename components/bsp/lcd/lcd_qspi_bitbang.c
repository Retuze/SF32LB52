#include "lcd_qspi_bitbang.h"

#include "bb_qspi.h"

static const bb_qspi_bus_t *lcd_qspi_bitbang_bus(void *ctx)
{
    return (const bb_qspi_bus_t *)ctx;
}

static void lcd_qspi_bitbang_init(void *ctx)
{
    bb_qspi_init(lcd_qspi_bitbang_bus(ctx));
}

static void lcd_qspi_bitbang_cs_low(void *ctx)
{
    bb_qspi_cs_low(lcd_qspi_bitbang_bus(ctx));
}

static void lcd_qspi_bitbang_cs_high(void *ctx)
{
    bb_qspi_cs_high(lcd_qspi_bitbang_bus(ctx));
}

static void lcd_qspi_bitbang_send_byte(void *ctx, uint8_t data)
{
    bb_qspi_send_byte(lcd_qspi_bitbang_bus(ctx), data);
}

static void lcd_qspi_bitbang_send(void *ctx, const uint8_t *data, uint16_t len)
{
    bb_qspi_send(lcd_qspi_bitbang_bus(ctx), data, len);
}

static void lcd_qspi_bitbang_send_4wire(void *ctx, const uint8_t *data, uint16_t len)
{
    bb_qspi_send_4wire(lcd_qspi_bitbang_bus(ctx), data, len);
}

static void lcd_qspi_bitbang_recv(void *ctx, uint8_t *data, uint16_t len)
{
    bb_qspi_recv(lcd_qspi_bitbang_bus(ctx), data, len);
}

const lcd_qspi_t lcd_qspi_bitbang_ops = {
    .init       = lcd_qspi_bitbang_init,
    .cs_low     = lcd_qspi_bitbang_cs_low,
    .cs_high    = lcd_qspi_bitbang_cs_high,
    .send_byte  = lcd_qspi_bitbang_send_byte,
    .send       = lcd_qspi_bitbang_send,
    .send_4wire = lcd_qspi_bitbang_send_4wire,
    .recv       = lcd_qspi_bitbang_recv,
};
