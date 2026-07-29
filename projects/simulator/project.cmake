# project.cmake — PC Simulator (Windows GDI / Linux X11, host clang)
#
# This project does NOT use the embedded toolchain.
# It links against the LithoUI framework compiled for the host,
# with platform-native windowing (X11 or GDI).
#
# Only active when SIMULATOR=ON is set in CMake cache.

if(NOT SIMULATOR)
    return()
endif()

set(PROJECT_LIBS
    lithoui
)
