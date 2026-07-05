# Simulator project — PC-hosted watch UI for rapid iteration
#
# Linux:   X11 native windowing
# Windows: GDI native windowing
# No SDL2 or other external dependencies required.

if(NOT SIMULATOR)
    return()
endif()

# ── Platform-specific sources ────────────────────────────────────────

if(WIN32)
    set(SIM_DISPLAY_SRC host_display_gdi.cpp)
    set(SIM_LIBS gdi32 user32)
else()
    find_package(X11 REQUIRED)
    set(SIM_DISPLAY_SRC host_display_x11.cpp)
    set(SIM_LIBS ${X11_LIBRARIES})
endif()

# ── Platform libs ─────────────────────────────────────────────────────

find_package(Threads REQUIRED)

# ── Executable ────────────────────────────────────────────────────────

add_executable(simulator
    main.cpp
    hal_stub.c
    host_input.cpp
    host_tick.cpp
    res_loader.cpp
    ${SIM_DISPLAY_SRC}
)

target_include_directories(simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(simulator PRIVATE
    litho
    Threads::Threads
    ${SIM_LIBS}
)

# ── Compile options ───────────────────────────────────────────────────

if(WIN32)
    # Windows: suppress min/max macros from windows.h
    target_compile_definitions(simulator PRIVATE NOMINMAX)
endif()

if(ENABLE_WARNINGS)
    target_compile_options(simulator PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ── Copy resource bundle alongside binary ─────────────────────────────

add_custom_command(TARGET simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/components/lithoui/generated/res_images.bin"
        "$<TARGET_FILE_DIR:simulator>/res_images.bin"
    COMMENT "Copying res_images.bin for simulator"
)

add_simulator(simulator)
