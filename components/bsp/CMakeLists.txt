# components/bsp — Board-level drivers (LCD, touch, sensors)
#
# These drivers use the HAL (via hal.h) for GPIO/pin control and bit-bang
# utilities for software protocol emulation.

add_library(bsp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd_qspi_bitbang.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/co5300_oled.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/nv3030b_tft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd_lcdc_co5300.c
)

target_include_directories(bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd       # lcd.h, lcd_qspi_bitbang.h
    ${PLATFORM_DIR}                      # board.h, LCD geometry / pins
)

target_link_libraries(bsp PUBLIC hal utility)

if(ENABLE_WARNINGS)
    target_compile_options(bsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
