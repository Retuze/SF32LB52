# components/hal — Chip-specific HAL for SF32LB52 (Cortex-M33)
#
# Layers (dependency order: SF32LB52.h → ll → hal_*):
#   SF32LB52.h   SoC register definitions, base addresses, bit fields
#   ll/          Low-level register drivers (thin wrappers over hardware)
#   hal_*.{c,h}  Higher-level peripheral abstractions (init/read/write patterns)
#
#   hal.h        Umbrella header — include this from application code.

add_library(hal STATIC
    # --- C++ runtime (freestanding) ---
    ${CMAKE_CURRENT_SOURCE_DIR}/cxx_runtime.cpp

    # --- LL drivers ---
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_atim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_dwt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_nvic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_pinmux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_rtt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_uart.c

    # --- HAL drivers ---
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_afio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_pwm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_uart.c

    # --- SDK LCDC port (hardware QSPI LCD output) ---
    ${CMAKE_CURRENT_SOURCE_DIR}/lcdc/bf0_hal_lcdc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcdc/hal_lcdc_port.c
)

target_compile_options(hal PUBLIC
    --target=arm-none-eabi
    -mcpu=cortex-m33
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    -mthumb
)

target_include_directories(hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}          # hal.h, SF32LB52.h, hal_*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ll       # ll_*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lcdc     # SDK LCDC HAL port
)

if(ENABLE_WARNINGS)
    target_compile_options(hal PRIVATE -Wall -Wextra -Wpedantic)
endif()
