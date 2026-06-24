# project.cmake — PC Simulator (Windows/Linux, host clang)
#
# This project does NOT use the embedded toolchain.
# It links against host libraries (SDL2 for display, pthread, etc.)
# for UI development and debugging without physical hardware.
#
# Only active when SIMULATOR=ON is set in CMake cache.

if(NOT SIMULATOR)
    return()
endif()

set(PROJECT_LIBS
    # No embedded libraries needed — simulator stubs out HAL/BSP
)
