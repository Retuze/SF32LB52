# Flash/SRAM/GPIO speed benchmark

add_executable(benchmark
    main.c
    ${CMAKE_SOURCE_DIR}/projects/firmware/startup.c
    ${CMAKE_SOURCE_DIR}/projects/firmware/system.c
    ${PLATFORM_DIR}/board.c
)

target_include_directories(benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PLATFORM_DIR}
)

target_link_libraries(benchmark PRIVATE hal bsp utility picolibc)
target_link_options(benchmark PRIVATE -T${PLATFORM_DIR}/link.ld)

if(ENABLE_WARNINGS)
    target_compile_options(benchmark PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_firmware(benchmark)
