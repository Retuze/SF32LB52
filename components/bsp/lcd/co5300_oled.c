#include "lcd.h"
#include "hal.h"
#include "board.h"

static const lcd_qspi_t *co5300_qspi(lcd_device_t *dev)
{
    return dev->bus->qspi;
}

static void *co5300_qspi_ctx(lcd_device_t *dev)
{
    return dev->bus->qspi_ctx;
}

static void co5300_mem_write(lcd_device_t *dev, const uint8_t *data, uint32_t len)
{
    const lcd_qspi_t *q = co5300_qspi(dev);
    void *ctx = co5300_qspi_ctx(dev);

    if (data == NULL || len == 0U) {
        return;
    }

    enum { MAXPX = 128U };
    uint8_t        rgb565[MAXPX * 2U];
    uint32_t       rem_px = len / 4U;
    const uint8_t *p      = data;

    if ((len % 4U) != 0U) {
        return;
    }

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
            uint16_t v = (uint16_t)(((uint16_t)(r & 0xF8U) << 8) |
                                    ((uint16_t)(g & 0xFCU) << 3) |
                                    ((uint16_t)b >> 3U));
            rgb565[(i * 2U) + 0U] = (uint8_t)(v >> 8);
            rgb565[(i * 2U) + 1U] = (uint8_t)(v & 0xFFU);
        }

        q->send_4wire(ctx, rgb565, (uint16_t)(chunk * 2U));
        p += chunk * 4U;
        rem_px -= chunk;
    }
}

/** Single-line DCS write (0x02 framing); all ic commands go through here. */
static void co5300_dcs_write(lcd_device_t *dev, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    lcd_write_cmd(dev, cmd, data, len);
}

static void co5300_dcs_read(lcd_device_t *dev, uint8_t cmd, uint8_t *data, uint16_t len)
{
    lcd_read_cmd(dev, cmd, data, len);
}

typedef struct {
    uint8_t           cmd;
    uint8_t           len;
    const uint8_t    *payload;
    uint16_t          delay_after_ms;
} co5300_init_step_t;

/** Payload blobs referenced by co5300_init_seq (ROM). */
static const uint8_t co5300_init_page20[]       = { 0x20U };
static const uint8_t co5300_init_f4_5a[]        = { 0x5AU };
static const uint8_t co5300_init_f5_59[]        = { 0x59U };
static const uint8_t co5300_init_f4_a5[]        = { 0xA5U };
static const uint8_t co5300_init_f5_a5[]        = { 0xA5U };
static const uint8_t co5300_init_page00[]       = { 0x00U };
static const uint8_t co5300_init_c4[]           = { 0x80U };
static const uint8_t co5300_init_colmod[]       = { DCS_COLMOD_RGB565 };
static const uint8_t co5300_init_tear_on[]      = { DCS_TEAR_OUTPUT_OFF };
static const uint8_t co5300_init_ctrl_display[] = { DCS_CTRL_DISP_BL_CTRL_EN };
static const uint8_t co5300_init_brightness[]   = { 0x80U };
static const uint8_t co5300_init_63[]           = { 0xFFU };
static const uint8_t co5300_init_col_window[]   = { 0x00U, 0x00U, (uint8_t)((LCD_WIDTH - 1U) >> 8), (uint8_t)((LCD_WIDTH - 1U) & 0xFFU) };
static const uint8_t co5300_init_row_window[]   = { 0x00U, 0x00U, (uint8_t)((LCD_HEIGHT - 1U) >> 8), (uint8_t)((LCD_HEIGHT - 1U) & 0xFFU) };

/**
 * Minimal bring-up; extend with vendor table from CO5300 datasheet.
 * Each row: DCS cmd, payload length, payload pointer (NULL if len == 0), delay after (ms).
 */
static const co5300_init_step_t co5300_init_seq[] = {
    { 0xFEU,                sizeof co5300_init_page20, co5300_init_page20, 0U },
    { 0xF4U,                sizeof co5300_init_f4_5a,  co5300_init_f4_5a,  0U },
    { 0xF5U,                sizeof co5300_init_f5_59,  co5300_init_f5_59,  0U },
    { 0xFEU,                sizeof co5300_init_page20, co5300_init_page20, 0U },
    { 0xF4U,                sizeof co5300_init_f4_a5,  co5300_init_f4_a5,  0U },
    { 0xF5U,                sizeof co5300_init_f5_a5,  co5300_init_f5_a5,  0U },
    { 0xFEU,                sizeof co5300_init_page00, co5300_init_page00, 0U },
    { 0xC4U,                sizeof co5300_init_c4,     co5300_init_c4,     0U },
    { DCS_SET_PIXEL_FORMAT, sizeof co5300_init_colmod, co5300_init_colmod, 0U },
    { DCS_SET_TEAR_ON,      sizeof co5300_init_tear_on, co5300_init_tear_on, 0U },
    { DCS_WRITE_CONTROL_DISPLAY, sizeof co5300_init_ctrl_display, co5300_init_ctrl_display, 0U },
    { DCS_SET_DISPLAY_BRIGHTNESS, sizeof co5300_init_brightness, co5300_init_brightness, 0U },
    { 0x63U,                sizeof co5300_init_63,     co5300_init_63,     0U },
    { DCS_SET_COLUMN_ADDRESS, sizeof co5300_init_col_window, co5300_init_col_window, 0U },
    { DCS_SET_PAGE_ADDRESS,   sizeof co5300_init_row_window, co5300_init_row_window, 0U },
    { DCS_EXIT_SLEEP_MODE,  0U,                        NULL,               120U },
    { DCS_SET_DISPLAY_ON,   0U,                        NULL,                20U },
};

static int co5300_init(lcd_device_t *dev)
{
    for (size_t i = 0U; i < sizeof(co5300_init_seq) / sizeof(co5300_init_seq[0]); i++) {
        const co5300_init_step_t *s = &co5300_init_seq[i];
        co5300_dcs_write(dev, s->cmd, (s->len > 0U) ? s->payload : NULL, s->len);
        if (s->delay_after_ms != 0U) {
            delay(s->delay_after_ms);
        }
    }
    return 0;
}

static void co5300_qspi_stream_start(lcd_device_t *dev)
{
    const lcd_qspi_t *q = co5300_qspi(dev);
    void *ctx = co5300_qspi_ctx(dev);
    /** 0x12 1-4-4 frame: cmd byte (1-wire), then address phase (4-wire).
     *  Reference: lcd_write_cmd_12 in the original SDK lcd.c. */
    const uint8_t framing[3] = { 0x00U, DCS_WRITE_MEMORY_START, 0x00U };

    q->cs_low(ctx);
    q->send_byte(ctx, LCD_QSPI_WRITE_Q);
    q->send_4wire(ctx, framing, 3U);
}

static void co5300_qspi_stream_end(lcd_device_t *dev)
{
    co5300_qspi(dev)->cs_high(co5300_qspi_ctx(dev));
}

static void co5300_set_window(lcd_device_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
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

    co5300_dcs_write(dev, DCS_SET_COLUMN_ADDRESS, col, sizeof col);
    co5300_dcs_write(dev, DCS_SET_PAGE_ADDRESS, row, sizeof row);
}

static void co5300_mem_write_begin(lcd_device_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (w == 0U || h == 0U) {
        return;
    }
    co5300_set_window(dev, x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    co5300_qspi_stream_start(dev);
}

static void co5300_mem_write_end(lcd_device_t *dev)
{
    co5300_qspi_stream_end(dev);
}

static void co5300_set_sleep(lcd_device_t *dev, int sleeping)
{
    if (sleeping != 0) {
        co5300_dcs_write(dev, DCS_ENTER_SLEEP_MODE, NULL, 0U);
    } else {
        co5300_dcs_write(dev, DCS_EXIT_SLEEP_MODE, NULL, 0U);
        delay(120U);
    }
}

static void co5300_set_brightness(lcd_device_t *dev, uint8_t percent)
{
    if (percent > 100U) {
        percent = 100U;
    }
    uint8_t level = (uint8_t)(((uint32_t)percent * 255U) / 100U);
    co5300_dcs_write(dev, DCS_SET_DISPLAY_BRIGHTNESS, &level, 1U);
}

static uint32_t co5300_read_id(lcd_device_t *dev)
{
    uint8_t id[3] = {0U, 0U, 0U};
    co5300_dcs_read(dev, DCS_GET_DISPLAY_ID, id, 3U);
    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | (uint32_t)id[2];
}

const lcd_ic_driver_t co5300_ic_driver = {
    .name            = "co5300",
    .init            = co5300_init,
    .set_window      = co5300_set_window,
    .mem_write_begin = co5300_mem_write_begin,
    .mem_write       = co5300_mem_write,
    .mem_write_end   = co5300_mem_write_end,
    .set_sleep       = co5300_set_sleep,
    .set_brightness  = co5300_set_brightness,
    .read_id         = co5300_read_id,
};
