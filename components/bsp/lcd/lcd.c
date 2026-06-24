#include "lcd.h"

#include "bb_gpio.h"
#include "hal.h"

static const lcd_qspi_t *lcd_qspi_ops(lcd_device_t *dev)
{
    return (dev != NULL && dev->bus != NULL) ? dev->bus->qspi : NULL;
}

static void *lcd_qspi_ctx(lcd_device_t *dev)
{
    return (dev != NULL && dev->bus != NULL) ? dev->bus->qspi_ctx : NULL;
}

void lcd_bus_init(lcd_device_t *dev)
{
    const lcd_qspi_t *q = lcd_qspi_ops(dev);
    if (q == NULL) {
        return;
    }

    q->init(lcd_qspi_ctx(dev));

    if (dev->pin_reset != LCD_PIN_NONE) {
        pinMode(dev->pin_reset, OUTPUT);
        digitalWrite(dev->pin_reset, HIGH);
    }

    if (dev->pin_te != LCD_PIN_NONE) {
        pinMode(dev->pin_te, INPUT);
    }

    if (dev->pin_bl != LCD_PIN_NONE) {
        pinMode(dev->pin_bl, OUTPUT);
        digitalWrite(dev->pin_bl, LOW);
    }
}

void lcd_bus_reset(lcd_device_t *dev)
{
    if (dev == NULL || dev->pin_reset == LCD_PIN_NONE) {
        return;
    }

    digitalWrite(dev->pin_reset, HIGH);
    delay(5U);
    digitalWrite(dev->pin_reset, LOW);
    delay(10U);
    digitalWrite(dev->pin_reset, HIGH);
    delay(120U);
}

void lcd_read_cmd(lcd_device_t *dev, uint8_t cmd, uint8_t *data, uint16_t len)
{
    const lcd_qspi_t *q = lcd_qspi_ops(dev);
    void *ctx = lcd_qspi_ctx(dev);
    if (q == NULL) {
        return;
    }

    q->cs_low(ctx);
    q->send_byte(ctx, LCD_QSPI_READ_SGL);
    q->send_byte(ctx, 0x00U);
    q->send_byte(ctx, cmd);
    q->send_byte(ctx, 0x00U);
    if (len > 0U && data != NULL) {
        q->recv(ctx, data, len);
    }
    q->cs_high(ctx);
}

void lcd_write_cmd(lcd_device_t *dev, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    const lcd_qspi_t *q = lcd_qspi_ops(dev);
    void *ctx = lcd_qspi_ctx(dev);
    if (q == NULL) {
        return;
    }

    q->cs_low(ctx);
    q->send_byte(ctx, LCD_QSPI_WRITE_SGL);
    q->send_byte(ctx, 0x00U);
    q->send_byte(ctx, cmd);
    q->send_byte(ctx, 0x00U);
    if (len > 0U && data != NULL) {
        q->send(ctx, data, len);
    }
    q->cs_high(ctx);
}

void lcd_write_cmd4(lcd_device_t *dev, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    const lcd_qspi_t *q = lcd_qspi_ops(dev);
    void *ctx = lcd_qspi_ctx(dev);
    if (q == NULL) {
        return;
    }

    q->cs_low(ctx);
    q->send_byte(ctx, LCD_QSPI_WRITE_Q);
    q->send_byte(ctx, 0x00U);
    q->send_byte(ctx, cmd);
    q->send_byte(ctx, 0x00U);
    if (len > 0U && data != NULL) {
        q->send_4wire(ctx, data, len);
    }
    q->cs_high(ctx);
}

void lcd_init(lcd_device_t *dev)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic == NULL) {
        return;
    }

    lcd_bus_init(dev);

    /**
     * OLED panels (set_brightness != NULL): the BL pin is often the panel
     * PMIC / ic supply — assert it HIGH *before* reset so the panel is
     * powered and the hardware-reset pulse takes effect.
     *
     * Reference: SDK lcd_test project's qspi_init() sets BL HIGH first,
     * then lcd_reset() toggles RST.
     */
    if (dev->pin_bl != LCD_PIN_NONE && ic->set_brightness != NULL) {
        digitalWrite(dev->pin_bl, HIGH);
    }

    lcd_bus_reset(dev);

    if (ic->init != NULL) {
        (void)ic->init(dev);
    }

    /**
     * TFT panels (set_brightness == NULL): BL is a backlight PWM enable.
     * Keep it LOW through init so the screen stays dark, then assert HIGH
     * when the first frame is ready.
     */
    if (dev->pin_bl != LCD_PIN_NONE && ic->set_brightness == NULL) {
        digitalWrite(dev->pin_bl, HIGH);
    }
}

void lcd_set_window(lcd_device_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic != NULL && ic->set_window != NULL) {
        ic->set_window(dev, x0, y0, x1, y1);
    }
}

void lcd_mem_write_begin(lcd_device_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic != NULL && ic->mem_write_begin != NULL) {
        ic->mem_write_begin(dev, x, y, w, h);
    }
}

void lcd_mem_write(lcd_device_t *dev, const uint8_t *data, uint32_t len)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic != NULL && ic->mem_write != NULL) {
        ic->mem_write(dev, data, len);
    }
}

void lcd_mem_write_end(lcd_device_t *dev)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic != NULL && ic->mem_write_end != NULL) {
        ic->mem_write_end(dev);
    }
}

void lcd_set_sleep(lcd_device_t *dev, int sleeping)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic != NULL && ic->set_sleep != NULL) {
        ic->set_sleep(dev, sleeping);
    }
}

void lcd_set_brightness(lcd_device_t *dev, uint8_t percent)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic != NULL && ic->set_brightness != NULL) {
        if (percent > 100U) {
            percent = 100U;
        }
        ic->set_brightness(dev, percent);
    }
}

uint32_t lcd_read_id(lcd_device_t *dev)
{
    const lcd_ic_driver_t *ic = (dev != NULL) ? dev->ic : NULL;
    if (ic != NULL && ic->read_id != NULL) {
        return ic->read_id(dev);
    }
    return 0U;
}

void lcd_draw_pixels(lcd_device_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint32_t *pixels_argb)
{
    if (dev == NULL || pixels_argb == NULL || w == 0U || h == 0U) {
        return;
    }

    lcd_mem_write_begin(dev, x, y, w, h);

    enum { CHUNK_PIXELS = 256U };
    const uint32_t *src  = pixels_argb;
    uint32_t        total = (uint32_t)w * (uint32_t)h;
    uint32_t        done  = 0U;

    while (done < total) {
        uint32_t n = total - done;
        if (n > CHUNK_PIXELS) {
            n = CHUNK_PIXELS;
        }
        lcd_mem_write(dev, (const uint8_t *)&src[done], n * 4U);
        done += n;
    }

    lcd_mem_write_end(dev);
}
