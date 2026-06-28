/**
 * @file lcd_ic_co5300.c
 * @brief CO5300 OLED panel IC driver.
 *
 * Default refresh: ~60Hz.  Define LCD_50HZ before including this file
 * (or via -D) to switch to ~50Hz for lower FPS targets.
 */

#include "lcd.h"
#include "hal.h"
#include <stdio.h>

static void ic_write(const lcd_bus_t *b, uint8_t cmd, const uint8_t *p, uint32_t n)
{
    b->begin();
    b->cmd_write(cmd, p, n);
    b->end();
}

static int co5300_init(const lcd_bus_t *b)
{
    /* Unlock */
    ic_write(b,0xFE, (const uint8_t*)"\x20", 1);
    ic_write(b,0xF4, (const uint8_t*)"\x5A", 1);
    ic_write(b,0xF5, (const uint8_t*)"\x59", 1);

#ifdef LCD_50HZ
    /* ≈50 Hz timing */
    ic_write(b,0xFE, (const uint8_t*)"\x40", 1);
    ic_write(b,0x39, (const uint8_t*)"\x24", 1);
    ic_write(b,0x3D, (const uint8_t*)"\x08", 1);
    ic_write(b,0x3F, (const uint8_t*)"\x00", 1);
    ic_write(b,0x40, (const uint8_t*)"\x5D", 1);
    ic_write(b,0xFE, (const uint8_t*)"\x70", 1);
    ic_write(b,0x65, (const uint8_t*)"\x16", 1);
    ic_write(b,0x66, (const uint8_t*)"\x53", 1);
    ic_write(b,0x67, (const uint8_t*)"\x10", 1);
    ic_write(b,0x8F, (const uint8_t*)"\xE7", 1);
    ic_write(b,0x90, (const uint8_t*)"\x45", 1);
    ic_write(b,0x91, (const uint8_t*)"\x80", 1);
#endif

    /* Re-lock + common init */
    ic_write(b,0xFE, (const uint8_t*)"\x20", 1);
    ic_write(b,0xF4, (const uint8_t*)"\xA5", 1);
    ic_write(b,0xF5, (const uint8_t*)"\xA5", 1);
    ic_write(b,0xFE, (const uint8_t*)"\x00", 1);

    ic_write(b,0xC4, (const uint8_t*)"\x80", 1);
    ic_write(b,0x3A, (const uint8_t*)"\x55", 1);
    ic_write(b,0x35, (const uint8_t*)"\x00", 1);  /* TE on */
    ic_write(b,0x53, (const uint8_t*)"\x20", 1);
    ic_write(b,0x51, (const uint8_t*)"\x80", 1);
    ic_write(b,0x63, (const uint8_t*)"\xFF", 1);
    ic_write(b,0x2A, (const uint8_t*)"\x00\x00\x01\x86", 4);
    ic_write(b,0x2B, (const uint8_t*)"\x00\x00\x01\xC2", 4);

    ic_write(b,0x11, NULL, 0);
    delay(120);
    ic_write(b,0x29, NULL, 0);
    printf("[co5300] init done\n");
    return 0;
}

static void co5300_set_window(const lcd_bus_t *b,
                              uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];
    buf[0] = (uint8_t)(x0 >> 8); buf[1] = (uint8_t)(x0 & 0xFF);
    buf[2] = (uint8_t)(x1 >> 8); buf[3] = (uint8_t)(x1 & 0xFF);
    ic_write(b,0x2A, buf, 4);

    buf[0] = (uint8_t)(y0 >> 8); buf[1] = (uint8_t)(y0 & 0xFF);
    buf[2] = (uint8_t)(y1 >> 8); buf[3] = (uint8_t)(y1 & 0xFF);
    ic_write(b,0x2B, buf, 4);
}

static uint32_t co5300_read_id(const lcd_bus_t *b)
{
    uint8_t id[3] = {0};
    b->begin();
    b->cmd_read(0x04, id, 3);
    b->end();
    printf("[co5300] ID: %02X %02X %02X\n", id[0], id[1], id[2]);
    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

const lcd_ic_t lcd_ic_co5300 = {
    .name       = "CO5300",
    .init       = co5300_init,
    .set_window = co5300_set_window,
    .sleep      = NULL,
    .read_id    = co5300_read_id,
};
