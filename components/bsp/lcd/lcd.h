#pragma once
#ifndef LCD_H
#define LCD_H

#include <stddef.h>
#include <stdint.h>

/** Optional pin: pass this when reset / TE are not wired. */
#define LCD_PIN_NONE (0xFFFFFFFFu)

/**
 * MIPI DCS command codes (shared by ic drivers over lcd_write_cmd / lcd_read_cmd).
 * Opcodes align with Linux `include/video/mipi_display.h` (MIPI DCS / DSI).
 */
#define DCS_NOP                          0x00U
#define DCS_SOFT_RESET                   0x01U
#define DCS_GET_COMPRESSION_MODE         0x03U
#define DCS_GET_DISPLAY_ID               0x04U
#define DCS_GET_ERROR_COUNT_ON_DSI       0x05U
#define DCS_GET_RED_CHANNEL              0x06U
#define DCS_GET_GREEN_CHANNEL            0x07U
#define DCS_GET_BLUE_CHANNEL             0x08U
#define DCS_GET_DISPLAY_STATUS           0x09U
#define DCS_GET_POWER_MODE               0x0AU
#define DCS_GET_ADDRESS_MODE             0x0BU
#define DCS_GET_PIXEL_FORMAT             0x0CU
#define DCS_GET_DISPLAY_MODE             0x0DU
#define DCS_GET_SIGNAL_MODE              0x0EU
#define DCS_GET_DIAGNOSTIC_RESULT        0x0FU
#define DCS_ENTER_SLEEP_MODE             0x10U
#define DCS_EXIT_SLEEP_MODE              0x11U
#define DCS_ENTER_PARTIAL_MODE           0x12U
#define DCS_ENTER_NORMAL_MODE            0x13U
#define DCS_GET_IMAGE_CHECKSUM_RGB       0x14U
#define DCS_GET_IMAGE_CHECKSUM_CT        0x15U
#define DCS_EXIT_INVERT_MODE             0x20U
#define DCS_ENTER_INVERT_MODE            0x21U
#define DCS_SET_GAMMA_CURVE              0x26U
#define DCS_SET_DISPLAY_OFF              0x28U
#define DCS_SET_DISPLAY_ON               0x29U
#define DCS_SET_COLUMN_ADDRESS           0x2AU
#define DCS_SET_PAGE_ADDRESS             0x2BU
#define DCS_WRITE_MEMORY_START           0x2CU
#define DCS_WRITE_LUT                    0x2DU
#define DCS_READ_MEMORY_START            0x2EU
#define DCS_SET_PARTIAL_ROWS             0x30U
#define DCS_SET_PARTIAL_COLUMNS          0x31U
#define DCS_SET_SCROLL_AREA              0x33U
#define DCS_SET_TEAR_OFF                 0x34U
#define DCS_SET_TEAR_ON                  0x35U
#define DCS_SET_ADDRESS_MODE             0x36U
#define DCS_SET_SCROLL_START             0x37U
#define DCS_EXIT_IDLE_MODE               0x38U
#define DCS_ENTER_IDLE_MODE              0x39U
#define DCS_SET_PIXEL_FORMAT             0x3AU
#define DCS_WRITE_MEMORY_CONTINUE        0x3CU
#define DCS_SET_3D_CONTROL               0x3DU
#define DCS_READ_MEMORY_CONTINUE         0x3EU
#define DCS_GET_3D_CONTROL               0x3FU
#define DCS_SET_VSYNC_TIMING             0x40U
#define DCS_SET_TEAR_SCANLINE            0x44U
#define DCS_GET_SCANLINE                 0x45U
#define DCS_SET_DISPLAY_BRIGHTNESS       0x51U
#define DCS_GET_DISPLAY_BRIGHTNESS       0x52U
#define DCS_WRITE_CONTROL_DISPLAY        0x53U
#define DCS_GET_CONTROL_DISPLAY          0x54U
#define DCS_WRITE_POWER_SAVE             0x55U
#define DCS_GET_POWER_SAVE               0x56U
#define DCS_SET_CABC_MIN_BRIGHTNESS      0x5EU
#define DCS_GET_CABC_MIN_BRIGHTNESS      0x5FU
#define DCS_READ_DDB_START               0xA1U
#define DCS_READ_PPS_START               0xA2U
#define DCS_READ_DDB_CONTINUE            0xA8U
#define DCS_READ_PPS_CONTINUE            0xA9U

/**
 * DCS_SET_PIXEL_FORMAT (0x3A): Linux names packed fields `MIPI_DCS_PIXEL_FMT_*` in
 * `include/video/mipi_display.h` (e.g. `MIPI_DCS_PIXEL_FMT_16BIT` = 5, `MIPI_DCS_PIXEL_FMT_24BIT` = 7).
 * Many SPI/QSPI ic datasheets instead use the byte values below as the first 0x3A parameter
 * (0x55 is also DCS opcode `DCS_WRITE_POWER_SAVE` — different role as cmd vs colmod payload).
 */
#define DCS_COLMOD_RGB565   0x55U
#define DCS_COLMOD_RGB666   0x66U
#define DCS_COLMOD_RGB888   0x77U
#define DCS_COLMOD_ARGB8888 0x88U

/** DCS 0x35 (Set Tear On) first parameter: 0 = off, 1 = tearing effect output on (see MIPI DCS). */
#define DCS_TEAR_OUTPUT_OFF 0x00U
#define DCS_TEAR_OUTPUT_ON  0x01U

/** DCS 0x26 (Set Gamma Curve) parameter: curve index 1–4 (ic-dependent). */
#define DCS_GAMMA_CURVE_1 0x01U
#define DCS_GAMMA_CURVE_2 0x02U
#define DCS_GAMMA_CURVE_3 0x03U
#define DCS_GAMMA_CURVE_4 0x04U

/** DCS 0x36 (Set Address Mode / MADCTL) bits — combine with |; RGB bit: 0 = BGR pixel order. */
#define DCS_MADCTL_MY  0x80U
#define DCS_MADCTL_MX  0x40U
#define DCS_MADCTL_MV  0x20U
#define DCS_MADCTL_ML  0x10U
#define DCS_MADCTL_RGB 0x08U
#define DCS_MADCTL_MH  0x04U

/** DCS 0x53 (Write Control Display) common bits (MIPI DCS; ic may ignore). */
#define DCS_CTRL_DISP_BL_CTRL_EN   0x20U
#define DCS_CTRL_DISP_DD_EN        0x04U
#define DCS_CTRL_DISP_DD_BRIGHT_EN 0x08U

/** Packed pixel format for 0x3A (same values as Linux `MIPI_DCS_PIXEL_FMT_*`). */
#define DCS_PIXEL_FMT_3BIT  1U
#define DCS_PIXEL_FMT_8BIT  2U
#define DCS_PIXEL_FMT_12BIT 3U
#define DCS_PIXEL_FMT_16BIT 5U
#define DCS_PIXEL_FMT_18BIT 6U
#define DCS_PIXEL_FMT_24BIT 7U

/** First SPI byte after CS low: line mode for lcd_read_cmd / lcd_write_cmd / lcd_write_cmd4. */
#define LCD_QSPI_WRITE_SGL 0x02U
#define LCD_QSPI_READ_SGL  0x03U
#define LCD_QSPI_WRITE_Q   0x12U

/** LCD-side QSPI transfer hooks (implementation owns GPIO / HW context). */
typedef struct lcd_qspi {
    void (*init)(void *ctx);
    void (*cs_low)(void *ctx);
    void (*cs_high)(void *ctx);
    void (*send_byte)(void *ctx, uint8_t data);
    void (*send)(void *ctx, const uint8_t *data, uint16_t len);
    void (*send_4wire)(void *ctx, const uint8_t *data, uint16_t len);
    void (*recv)(void *ctx, uint8_t *data, uint16_t len);
} lcd_qspi_t;

/** ic bus: transport ops + implementation-private context. */
typedef struct {
    const lcd_qspi_t *qspi;
    void             *qspi_ctx;
} lcd_bus_t;

typedef struct lcd_device lcd_device_t;
typedef struct lcd_ic_driver lcd_ic_driver_t;

struct lcd_ic_driver {
    const char *name;
    int (*init)(lcd_device_t *dev);
    void (*set_window)(lcd_device_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void (*mem_write_begin)(lcd_device_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void (*mem_write)(lcd_device_t *dev, const uint8_t *data, uint32_t len);
    void (*mem_write_end)(lcd_device_t *dev);
    void (*set_sleep)(lcd_device_t *dev, int sleeping);
    /** NULL: brightness via BL PWM; non-NULL: DCS etc. (BL pin may be supply, see lcd_device.pin_bl). */
    void (*set_brightness)(lcd_device_t *dev, uint8_t percent);
    uint32_t (*read_id)(lcd_device_t *dev);
};

struct lcd_device {
    const char *name;
    lcd_bus_t *bus;
    const lcd_ic_driver_t *ic;
    uint16_t width;
    uint16_t height;
    uint32_t pin_reset;
    uint32_t pin_te;
    /**
     * Active high. Meaning depends on driver->set_brightness:
     * NULL — TFT: BL / PWM enable; held low through init, then high.
     * non-NULL — OLED: often PMIC / ic supply; asserted high before ic driver init.
     */
    uint32_t pin_bl;
    void *driver_data;
};

void lcd_bus_init(lcd_device_t *dev);
void lcd_bus_reset(lcd_device_t *dev);

/** QSPI 0x03: command on D0 (single), parameters read on D0 (single). */
void lcd_read_cmd(lcd_device_t *dev, uint8_t cmd, uint8_t *data, uint16_t len);

/** QSPI 0x02: command + parameters on D0 (single-line). */
void lcd_write_cmd(lcd_device_t *dev, uint8_t cmd, const uint8_t *data, uint16_t len);

/** QSPI 0x12: command byte on D0 (single), parameters on D0–D3 (quad). */
void lcd_write_cmd4(lcd_device_t *dev, uint8_t cmd, const uint8_t *data, uint16_t len);

void lcd_init(lcd_device_t *dev);
void lcd_set_window(lcd_device_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_mem_write_begin(lcd_device_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void lcd_mem_write(lcd_device_t *dev, const uint8_t *data, uint32_t len);
void lcd_mem_write_end(lcd_device_t *dev);
void lcd_set_sleep(lcd_device_t *dev, int sleeping);
void lcd_set_brightness(lcd_device_t *dev, uint8_t percent);
uint32_t lcd_read_id(lcd_device_t *dev);
/** @param pixels_argb ARGB8888 (0xAARRGGBB per uint32); bus sends 4 bytes/pixel (DCS_SET_PIXEL_FORMAT / colmod must match). */
void lcd_draw_pixels(lcd_device_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint32_t *pixels_argb);

#endif
