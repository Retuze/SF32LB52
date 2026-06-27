# LithoUI — lightweight embedded C++ UI framework

add_library(litho STATIC
    # --- resource embed (assembly .incbin) ---
    generated/res_embed.S

    # --- framework ---
    framework/view/view.cpp
    framework/activity/activity.cpp
    framework/intent/bundle.cpp
)

# res_embed.S pulls in res_images.bin via `.incbin`. Assembler .incbin
# dependencies are NOT auto-tracked by CMake/Ninja, so a regenerated bundle
# would NOT trigger a rebuild — the firmware would keep the stale image data
# that was current when res_embed.S was last compiled (mismatching res_images.h).
# Declare the dependency explicitly so the bundle is always re-embedded.
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

if(ENABLE_WARNINGS)
    target_compile_options(litho PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
