# Firmware project — main watch application

add_executable(firmware
    main.c
    startup.c
    system.c
    ${PLATFORM_DIR}/board.c
    ${PLATFORM_DIR}/lcd_ref.c
)

target_include_directories(firmware PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PLATFORM_DIR}
)

target_link_libraries(firmware PRIVATE hal bsp utility picolibc)
target_link_options(firmware PRIVATE -T${PLATFORM_DIR}/link.ld)

if(ENABLE_WARNINGS)
    target_compile_options(firmware PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
    )
endif()

add_firmware(firmware)
