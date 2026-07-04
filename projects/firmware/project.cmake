# project.cmake — Main firmware (watch application with LithoUI)
#
# Available libraries (registered in cmake/libraries.cmake):
#   hal       — SoC headers, LL + HAL drivers
#   bsp       — LCD, touch, sensor board-level drivers
#   utility   — bit-bang, logging, ring buffer
#   litho     — LithoUI framework
#   picolibc  — C standard library + compiler-rt

set(PROJECT_LIBS
    hal
    bsp
    utility
    lithoui
    picolibc
)

# ---- platform selection ------------------------------------------------
set(PLATFORM "watch_v1_0" CACHE STRING "Target hardware variant")
set(PLATFORM_DIR "${CMAKE_SOURCE_DIR}/platforms/${PLATFORM}")
