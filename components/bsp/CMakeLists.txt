# components/bsp — Board-level drivers (LCD, touch)
#
# LCD bus selection: cmake --preset debug -DLCD_BUS=GPIO
#   LCDC (default) — hardware QSPI + DMA
#   GPIO           — bit-bang QSPI

if(NOT DEFINED LCD_BUS)
    set(LCD_BUS "LCDC" CACHE STRING "LCD bus driver: LCDC or GPIO")
endif()

add_library(bsp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/ic/lcd_ic_co5300.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tp/tp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tp/bus/tp_bus_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tp/ic/tp_ic_ft6146.c
)

if(LCD_BUS STREQUAL "LCDC")
    target_sources(bsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lcd/bus/lcd_bus_qspi_lcdc.c)
    target_compile_definitions(bsp PUBLIC LCD_BUS_LCDC=1)
elseif(LCD_BUS STREQUAL "GPIO")
    target_sources(bsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lcd/bus/lcd_bus_qspi_gpio.c)
    target_compile_definitions(bsp PUBLIC LCD_BUS_GPIO=1)
else()
    message(FATAL_ERROR "LCD_BUS must be 'LCDC' or 'GPIO', got '${LCD_BUS}'")
endif()

target_include_directories(bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd        # lcd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tp         # tp.h
    ${PLATFORM_DIR}                       # board.h
)

target_link_libraries(bsp PUBLIC hal utility picolibc)

if(ENABLE_WARNINGS)
    target_compile_options(bsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
