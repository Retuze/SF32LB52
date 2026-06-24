# Simulator project — PC-hosted watch UI for rapid iteration

if(NOT SIMULATOR)
    return()
endif()

find_package(SDL2 QUIET)
find_package(Threads REQUIRED)

add_executable(simulator
    main.c
    hal_stub.c
)

target_include_directories(simulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(simulator PRIVATE Threads::Threads)

if(SDL2_FOUND)
    target_compile_definitions(simulator PRIVATE HAS_SDL2=1)
    target_include_directories(simulator PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(simulator PRIVATE ${SDL2_LIBRARIES})
else()
    message(WARNING "SDL2 not found — simulator will be headless (no display)")
endif()

if(ENABLE_WARNINGS)
    target_compile_options(simulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_simulator(simulator)
