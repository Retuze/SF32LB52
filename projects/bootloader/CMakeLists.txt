# Bootloader project — OTA + firmware jump

add_executable(bootloader
    main.c
    startup.c
    system.c
    flash_quad_init.c
)

target_include_directories(bootloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PLATFORM_DIR}
)

target_link_libraries(bootloader PRIVATE hal picolibc)
target_link_options(bootloader PRIVATE -T${CMAKE_CURRENT_SOURCE_DIR}/link.ld)

if(ENABLE_WARNINGS)
    target_compile_options(bootloader PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
    )
endif()

add_firmware(bootloader)
