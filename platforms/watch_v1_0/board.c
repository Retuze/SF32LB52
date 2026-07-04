/**
 * @file board.c
 * @brief Watch V1.0 board initialization — LCD & TP configuration.
 */
#include "board.h"
#include "lcd.h"

/* ── Board-level LCD initialization ──────────────────────────────────────── */

/* Forward declarations from bsp drivers */
extern const lcd_bus_t lcd_bus_qspi_gpio;
extern const lcd_ic_t  lcd_ic_co5300;

void board_lcd_init(void)
{
    lcd_set_bus(&lcd_bus_qspi_gpio);
    lcd_set_ic(&lcd_ic_co5300);
    lcd_set_resolution(LCD_WIDTH, LCD_HEIGHT);
    lcd_set_ctrl_pins(LCD_RST, LCD_BL);
    lcd_init();
}
