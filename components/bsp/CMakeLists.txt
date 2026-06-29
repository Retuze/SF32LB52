# components/bsp — Board-level drivers (LCD, touch)
#
# LCD driver: components/bsp/lcd/ (framework: bus + IC)
# Touch driver: components/bsp/tp/

add_library(bsp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd.c
    # Bus driver: choose ONE of the two below
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd_bus_qspi.c        # bit-bang QSPI (default)
    # ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd_bus_qspi_lcdc.c  # LCDC hardware QSPI (async DMA)
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd/lcd_ic_co5300.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tp/tp_ft6146.c
)

target_include_directories(bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd        # lcd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tp         # tp_ft6146.h
    ${PLATFORM_DIR}                       # board.h
)

target_link_libraries(bsp PUBLIC hal utility picolibc)

if(ENABLE_WARNINGS)
    target_compile_options(bsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
