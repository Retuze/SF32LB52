# ftab — Flash partition table generator (host executable)
#
# This is a *host* tool, not cross-compiled for the target.
# Output: ftab.bin (32 KB binary containing sec_config structure)

if(CMAKE_CROSSCOMPILING)
    # When cross-compiling, we need to build this tool with the host compiler.
    # Find the host C compiler (try common names)
    find_program(HOST_CC NAMES gcc clang cc cl REQUIRED)

    message(STATUS "ftab: cross-compile detected, using host compiler: ${HOST_CC}")

    set(FTAB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.c")

    # On Windows, clang adds .exe automatically, so we need to handle both paths
    if(WIN32)
        set(FTAB_EXE "${CMAKE_CURRENT_BINARY_DIR}/ftab_gen.exe")
    else()
        set(FTAB_EXE "${CMAKE_CURRENT_BINARY_DIR}/ftab_gen")
    endif()

    # Build the host tool at configure time
    execute_process(
        COMMAND "${HOST_CC}" -O2 "${FTAB_SRC}" -o "${FTAB_EXE}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _error
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "ftab: failed to build host tool with ${HOST_CC}:\n${_error}")
    endif()

    # Generate ftab.bin using the host tool
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/ftab.bin"
        COMMAND "${FTAB_EXE}" "${CMAKE_BINARY_DIR}/ftab.bin"
        DEPENDS "${FTAB_SRC}"
        COMMENT "Generating ftab.bin"
        VERBATIM
    )

else()
    # Native build: use normal add_executable
    add_executable(ftab_gen main.c)

    set_target_properties(ftab_gen PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    # Generate ftab.bin by running the host executable
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/ftab.bin"
        COMMAND ftab_gen "${CMAKE_BINARY_DIR}/ftab.bin"
        DEPENDS ftab_gen
        COMMENT "Generating ftab.bin"
        VERBATIM
    )
endif()

# Make ftab.bin a build target
add_custom_target(ftab ALL DEPENDS "${CMAKE_BINARY_DIR}/ftab.bin")
