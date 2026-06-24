# LithoUI demo project

add_executable(firmware-litho
    main.cpp
    ${PLATFORM_DIR}/lcd_ref.c
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

add_firmware(firmware-litho)
