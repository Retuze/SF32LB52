# LithoUI demo project
#
#   cmake --preset debug -DLCD_50HZ=ON   → 50Hz display
#   cmake --preset debug                   → 60Hz (default)

option(LCD_50HZ "Set LCD refresh to ~50Hz" OFF)

add_executable(firmware-litho
    main.cpp
    ${CMAKE_SOURCE_DIR}/projects/firmware/startup.c
    ${CMAKE_SOURCE_DIR}/projects/firmware/system.c
    ${PLATFORM_DIR}/board.c
)

target_include_directories(firmware-litho PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PLATFORM_DIR}
)

target_link_libraries(firmware-litho PRIVATE hal bsp utility litho picolibc)
target_link_options(firmware-litho PRIVATE -T${PLATFORM_DIR}/link.ld)

if(ENABLE_WARNINGS)
    target_compile_options(firmware-litho PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()

if(LCD_50HZ)
    target_compile_definitions(firmware-litho PRIVATE -DLCD_50HZ)
    target_compile_definitions(bsp PRIVATE -DLCD_50HZ)   # lcd_ic_co5300.c needs it
endif()

add_firmware(firmware-litho)
