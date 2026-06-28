# components/utils — Platform-agnostic utilities
#
#   bitbang/   Software-emulated communication protocols (I2C, SPI, QSPI, UART)
#   log/       Conditional printf-based logging (LOGT/LOGD/LOGI/LOGW/LOGE/LOGF)

add_library(utility STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/bitbang/bb_qspi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bitbang/bb_spi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bitbang/bb_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bitbang/bb_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/segger_rtt.c
)

target_include_directories(utility PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}            # segger_rtt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bitbang    # bb_*.h, bb_gpio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/log        # log.h
)

target_link_libraries(utility PUBLIC hal)

if(ENABLE_WARNINGS)
    target_compile_options(utility PRIVATE -Wall -Wextra -Wpedantic)
endif()
