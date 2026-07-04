# components/bsp — Board-level drivers (LCD, touch)
#
# LCD driver: components/bsp/lcd/ (framework: bus + IC)
# Touch driver: components/bsp/tp/ (framework: bus + IC)

add_library(bsp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/bus/lcd_bus_qspi_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/bus/lcd_bus_qspi_lcdc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/ic/lcd_ic_co5300.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tp/tp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tp/bus/tp_bus_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tp/ic/tp_ic_ft6146.c
)

target_include_directories(bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd        # lcd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tp         # tp.h
    ${PLATFORM_DIR}                       # board.h
)

target_link_libraries(bsp PUBLIC hal utility picolibc)

if(ENABLE_WARNINGS)
    target_compile_options(bsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
