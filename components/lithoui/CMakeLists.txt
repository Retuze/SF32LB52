# LithoUI — lightweight embedded C++ UI framework

add_library(litho STATIC
    # --- resource embed (assembly .incbin) ---
    generated/res_embed.S

    # --- framework ---
    framework/view/view.cpp
    framework/activity/activity.cpp
    framework/intent/bundle.cpp
)

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
