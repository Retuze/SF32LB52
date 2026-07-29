# LithoUI — lightweight embedded C++ UI framework
#
# Two build modes:
#   SIMULATOR=OFF  → ARM firmware (arm-none-eabi, hal/picolibc, .incbin)
#   SIMULATOR=ON   → Host PC (native clang, no HAL, runtime resource loader)

if(SIMULATOR)
    # ── Host build (PC simulator) ──────────────────────────────────
    add_library(litho STATIC
        framework/view/view.cpp
        framework/activity/activity.cpp
        framework/intent/bundle.cpp
    )

    target_include_directories(litho PUBLIC
        ${CMAKE_SOURCE_DIR}/components/lithoui
        ${CMAKE_SOURCE_DIR}/components/lithoui/generated
    )
    # Stub hal.h intercepts `#include "hal.h"` from painter.hpp/pfb.hpp
    target_include_directories(litho SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/projects/simulator/hal_stub
    )

else()
    # ── ARM firmware build ─────────────────────────────────────────
    add_library(litho STATIC
        generated/res_embed.S
        framework/view/view.cpp
        framework/activity/activity.cpp
        framework/intent/bundle.cpp
    )

    # res_embed.S pulls in res_images.bin via `.incbin`. Assembler .incbin
    # dependencies are NOT auto-tracked by CMake/Ninja, so a regenerated bundle
    # would NOT trigger a rebuild. Declare the dependency explicitly.
    set_source_files_properties(generated/res_embed.S PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/res_images.bin")

    target_include_directories(litho PUBLIC
        ${CMAKE_SOURCE_DIR}/components/lithoui
        ${CMAKE_SOURCE_DIR}/components/lithoui/port/sf32lb52
        ${CMAKE_SOURCE_DIR}/components/lithoui/generated
    )

    target_link_libraries(litho PUBLIC hal picolibc)

    target_compile_options(litho PRIVATE
        --target=arm-none-eabi
        -mcpu=cortex-m33
        -mfpu=fpv5-sp-d16
        -mfloat-abi=hard
        -mthumb
    )
endif()

# Alias so projects can link as either `litho` or `lithoui`
add_library(lithoui ALIAS litho)

if(ENABLE_WARNINGS)
    target_compile_options(litho PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
