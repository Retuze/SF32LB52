# project.cmake — Bootloader (< 32 KB, separate link script)
#
# Minimal dependency set: only HAL + picolibc.
# The bootloader runs before the main firmware, handles OTA updates,
# and jumps to the application at 0x12020000.

set(PROJECT_LIBS
    hal
    picolibc
)

set(PLATFORM "watch_v1_0" CACHE STRING "Target hardware variant")
set(PLATFORM_DIR "${CMAKE_SOURCE_DIR}/platforms/${PLATFORM}")
