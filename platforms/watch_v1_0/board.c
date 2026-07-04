/**
 * @file board.c
 * @brief Watch V1.0 board initialization — LCD & TP configuration.
 */
#include "board.h"
#include "lcd.h"
#include "tp.h"
#include "bb_i2c.h"

/* ── Board-level LCD initialization ──────────────────────────────────────── */

/* Forward declarations from bsp drivers */
extern const lcd_bus_t lcd_bus_qspi_gpio;
extern const lcd_ic_t  lcd_ic_co5300;

void board_lcd_init(void)
{
    lcd_set_bus(&lcd_bus_qspi_gpio);
    lcd_set_ic(&lcd_ic_co5300);
    lcd_set_ctrl_pins(LCD_RST, LCD_BL);
    lcd_init();
}

/* ── Board-level TP initialization ───────────────────────────────────────── */

/* Forward declarations from bsp drivers */
extern const tp_bus_t tp_bus_i2c;
extern const tp_ic_t  tp_ic_ft6146;

/* I2C half-period delay (~250 kHz I2C clock) */
static void tp_i2c_delay(void)
{
    for (volatile uint32_t i = 0U; i < 40U; ++i) {
        __asm volatile("nop");
    }
}

/* I2C bus configuration for touch panel */
static const bb_i2c_t tp_i2c_config = {
    .pin_sda = CTP_SDA,
    .pin_scl = CTP_SCL,
    .half_period = tp_i2c_delay,
};

void board_tp_init(void)
{
    tp_set_bus(&tp_bus_i2c, &tp_i2c_config);
    tp_set_ic(&tp_ic_ft6146);
    tp_set_ctrl_pins(CTP_RST, CTP_INT);
    tp_init();
}
