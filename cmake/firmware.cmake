# firmware.cmake — helpers for bare-metal firmware targets
#
# add_firmware(<target>)
#   Sets up an ELF executable target with:
#   - post-build: .bin, .hex generation + size report
#   - linker: --gc-sections, .map output

function(add_firmware _FW_TARGET)
    set_target_properties(${_FW_TARGET} PROPERTIES
        SUFFIX ".elf"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    target_link_options(${_FW_TARGET} PRIVATE
        -nostdlib
        -Wl,--gc-sections
        -Wl,--build-id=none
        -Wl,-Map,$<TARGET_FILE_DIR:${_FW_TARGET}>/${_FW_TARGET}.map
    )

    add_custom_command(
        TARGET ${_FW_TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${_FW_TARGET}> $<TARGET_FILE_DIR:${_FW_TARGET}>/${_FW_TARGET}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${_FW_TARGET}> $<TARGET_FILE_DIR:${_FW_TARGET}>/${_FW_TARGET}.hex
        COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${_FW_TARGET}>
        VERBATIM
    )
endfunction()

# add_simulator(<target>)
#   Sets up a host (Windows/Linux) executable for PC-based UI debugging.
function(add_simulator _FW_TARGET)
    set_target_properties(${_FW_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endfunction()
