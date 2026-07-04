/**
 * @file board.c
 * @brief Watch V1.0 board initialization — LCD & TP configuration.
 */
#include "board.h"
#include "lcd.h"
#include "tp.h"

/* ── Board-level LCD initialization ──────────────────────────────────────── */

/* Forward declarations from bsp drivers */
#if defined(LCD_BUS_GPIO)
extern const lcd_bus_t lcd_bus_qspi_gpio;
#elif defined(LCD_BUS_LCDC)
extern const lcd_bus_t lcd_bus_qspi_lcdc;
#endif
extern const lcd_ic_t  lcd_ic_co5300;

void board_lcd_init(void)
{
#if defined(LCD_BUS_GPIO)
    lcd_set_bus(&lcd_bus_qspi_gpio);
#elif defined(LCD_BUS_LCDC)
    lcd_set_bus(&lcd_bus_qspi_lcdc);
#endif
    lcd_set_ic(&lcd_ic_co5300);
    lcd_set_ctrl_pins(LCD_RST, LCD_BL);
    lcd_init();
}

/* ── Board-level TP initialization ───────────────────────────────────────── */

/* Forward declarations from bsp drivers */
extern const tp_bus_t tp_bus_i2c;
extern const tp_ic_t  tp_ic_ft6146;

void board_tp_init(void)
{
    tp_set_bus(&tp_bus_i2c);
    tp_set_ic(&tp_ic_ft6146);
    tp_set_ctrl_pins(CTP_RST, CTP_INT);
    tp_init();
}
