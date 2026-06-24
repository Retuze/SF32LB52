#include "lcd.h"
#include "hal.h"

static const lcd_qspi_t *nv3030b_qspi(lcd_device_t *dev)
{
    return dev->bus->qspi;
}

static void *nv3030b_qspi_ctx(lcd_device_t *dev)
{
    return dev->bus->qspi_ctx;
}

static void nv3030b_mem_write(lcd_device_t *dev, const uint8_t *data, uint32_t len)
{
    const lcd_qspi_t *q = nv3030b_qspi(dev);
    void *ctx = nv3030b_qspi_ctx(dev);

    if (data == NULL || len == 0U || (len % 4U) != 0U) {
        return;
    }

    enum { MAXPX = 128U };
    uint16_t       rgb565[MAXPX];
    uint32_t       rem_px = len / 4U;
    const uint8_t *p     = data;

    while (rem_px > 0U) {
        uint32_t chunk = rem_px;
        if (chunk > MAXPX) {
            chunk = MAXPX;
        }
        const uint32_t *px = (const uint32_t *)p;
        for (uint32_t i = 0U; i < chunk; i++) {
            uint32_t c = px[i];
            uint8_t  r = (uint8_t)((c >> 16) & 0xFFU);
            uint8_t  g = (uint8_t)((c >> 8) & 0xFFU);
            uint8_t  b = (uint8_t)(c & 0xFFU);
            rgb565[i]  = (uint16_t)(((uint16_t)(r & 0xF8U) << 8) | ((uint16_t)(g & 0xFCU) << 3) |
                                    ((uint16_t)(b >> 3U)));
        }
        q->send_4wire(ctx, (const uint8_t *)rgb565, (uint16_t)(chunk * 2U));
        p += chunk * 4U;
        rem_px -= chunk;
    }
}

/** Single-line DCS write (0x02 framing); all ic commands go through here. */
static void nv3030b_dcs_write(lcd_device_t *dev, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    lcd_write_cmd(dev, cmd, data, len);
}

static void nv3030b_dcs_read(lcd_device_t *dev, uint8_t cmd, uint8_t *data, uint16_t len)
{
    lcd_read_cmd(dev, cmd, data, len);
}

/**
 * Init blob format: cmd, (arg_count | 0x80 for delay 150 ms after), args…
 * Adapted from Adafruit_NV3030B (BSD). Vendor opcodes are raw bytes; DCS-aligned
 * steps use lcd.h `DCS_*` names only in the tail (same numeric values as Adafruit).
 */
static const uint8_t nv3030b_initcmd[] = {
    0xFD, 2, 0x06, 0x08, 0x61, 2, 0x07, 0x04, 0x62, 4, 0x00, 0x44, 0x40, 0x00, 0x63, 4, 0x41, 0x07, 0x12, 0x12,
    0x64, 1, 0x37, 0x65, 3, 0x09, 0x10, 0x21, 0x66, 3, 0x09, 0x10, 0x21, 0x67, 2, 0x20, 0x40, 0x68, 4, 0x90, 0x4C,
    0x7C, 0x06, 0xB1, 3, 0x0F, 0x02, 0x01, 0xB4, 1, 0x01, 0xB5, 4, 0x02, 0x02, 0x0A, 0x14, 0xB6, 5, 0x04, 0x01,
    0x9F, 0x00, 0x02, 0xDF, 1, 0x11, 0xE2, 6, 0x03, 0x00, 0x00, 0x26, 0x27, 0x3F, 0xE5, 6, 0x3F, 0x27, 0x26, 0x00,
    0x00, 0x03, 0xE1, 2, 0x00, 0x57, 0xE4, 2, 0x58, 0x00, 0xE0, 8, 0x01, 0x03, 0x0D, 0x0E, 0x0E, 0x0C, 0x15, 0x19,
    0xE3, 8, 0x1A, 0x16, 0x0C, 0x0F, 0x0E, 0x0D, 0x02, 0x01, 0xE6, 2, 0x00, 0xFF, 0xE7, 6, 0x01, 0x04, 0x03, 0x03,
    0x00, 0x12, 0xE8, 3, 0x00, 0x70, 0x00, 0xEC, 1, 0x52, 0xF1, 3, 0x01, 0x01, 0x02, 0xF6, 4, 0x09, 0x10, 0x00, 0x00,
    0xFD, 2, 0xFA, 0xFC, (uint8_t)DCS_SET_PIXEL_FORMAT, 1, 0x05, (uint8_t)DCS_SET_TEAR_ON, 1, 0x00,
    (uint8_t)DCS_SET_ADDRESS_MODE, 1, 0x08, (uint8_t)DCS_ENTER_INVERT_MODE, 0, (uint8_t)DCS_EXIT_SLEEP_MODE, 0x80,
    (uint8_t)DCS_SET_DISPLAY_ON, 0x80, 0x00};

static void nv3030b_run_initcmd_table(lcd_device_t *dev)
{
    const uint8_t *addr = nv3030b_initcmd;
    for (;;) {
        uint8_t cmd = *addr++;
        if (cmd == 0U) {
            break;
        }
        uint8_t x        = *addr++;
        uint8_t num_args = x & 0x7FU;
        nv3030b_dcs_write(dev, cmd, addr, num_args);
        addr += num_args;
        if ((x & 0x80U) != 0U) {
            delay(150U);
        }
    }
}

static int nv3030b_init(lcd_device_t *dev)
{
    nv3030b_run_initcmd_table(dev);
    return 0;
}

static void nv3030b_qspi_stream_start(lcd_device_t *dev)
{
    const lcd_qspi_t *q = nv3030b_qspi(dev);
    void *ctx = nv3030b_qspi_ctx(dev);

    q->cs_low(ctx);
    q->send_byte(ctx, LCD_QSPI_WRITE_Q);
    q->send_byte(ctx, DCS_WRITE_MEMORY_START);
}

static void nv3030b_qspi_stream_end(lcd_device_t *dev)
{
    nv3030b_qspi(dev)->cs_high(nv3030b_qspi_ctx(dev));
}

static void nv3030b_set_window(lcd_device_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t col[4] = {
        (uint8_t)(x0 >> 8),
        (uint8_t)(x0 & 0xFFU),
        (uint8_t)(x1 >> 8),
        (uint8_t)(x1 & 0xFFU),
    };
    uint8_t row[4] = {
        (uint8_t)(y0 >> 8),
        (uint8_t)(y0 & 0xFFU),
        (uint8_t)(y1 >> 8),
        (uint8_t)(y1 & 0xFFU),
    };

    nv3030b_dcs_write(dev, DCS_SET_COLUMN_ADDRESS, col, sizeof col);
    nv3030b_dcs_write(dev, DCS_SET_PAGE_ADDRESS, row, sizeof row);
}

static void nv3030b_mem_write_begin(lcd_device_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (w == 0U || h == 0U) {
        return;
    }
    nv3030b_set_window(dev, x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    nv3030b_qspi_stream_start(dev);
}

static void nv3030b_mem_write_end(lcd_device_t *dev)
{
    nv3030b_qspi_stream_end(dev);
}

static void nv3030b_set_sleep(lcd_device_t *dev, int sleeping)
{
    if (sleeping != 0) {
        nv3030b_dcs_write(dev, DCS_ENTER_SLEEP_MODE, NULL, 0U);
    } else {
        nv3030b_dcs_write(dev, DCS_EXIT_SLEEP_MODE, NULL, 0U);
        delay(120U);
    }
}

static void nv3030b_set_brightness(lcd_device_t *dev, uint8_t percent)
{
    if (percent > 100U) {
        percent = 100U;
    }
    uint8_t level = (uint8_t)(((uint32_t)percent * 255U) / 100U);
    nv3030b_dcs_write(dev, DCS_SET_DISPLAY_BRIGHTNESS, &level, 1U);
}

static uint32_t nv3030b_read_id(lcd_device_t *dev)
{
    uint8_t id[3] = {0U, 0U, 0U};
    nv3030b_dcs_read(dev, DCS_GET_DISPLAY_ID, id, 3U);
    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | (uint32_t)id[2];
}

const lcd_ic_driver_t nv3030b_ic_driver = {
    .name            = "nv3030b",
    .init            = nv3030b_init,
    .set_window      = nv3030b_set_window,
    .mem_write_begin = nv3030b_mem_write_begin,
    .mem_write       = nv3030b_mem_write,
    .mem_write_end   = nv3030b_mem_write_end,
    .set_sleep       = nv3030b_set_sleep,
    .set_brightness  = nv3030b_set_brightness,
    .read_id         = nv3030b_read_id,
};
