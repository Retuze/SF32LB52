# components/hal — Chip-specific HAL for SF32LB52 (Cortex-M33)
#
#   SF32LB52.h   SoC register definitions, base addresses, bit fields
#   ll/          LL drivers: registers, clock, NVIC, pinmux, GPIO, UART
#   hal_*.{c,h}  Higher-level drivers: PWM
#
#   hal.h        Umbrella header — include this from application code.

add_library(hal STATIC
    # --- C++ runtime (freestanding) ---
    ${CMAKE_CURRENT_SOURCE_DIR}/cxx_runtime.cpp

    # --- LL drivers ---
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_atim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/clock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_pinmux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ll/ll_uart.c

    # --- HAL drivers ---
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_pwm.c

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
    ${CMAKE_CURRENT_SOURCE_DIR}/lcdc     # bf0_hal_lcdc.h, bf0_hal.h, register.h, ...
)

if(ENABLE_WARNINGS)
    target_compile_options(hal PRIVATE -Wall -Wextra -Wpedantic)
endif()
