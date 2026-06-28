# libraries.cmake — library registry with automatic dependency resolution
#
# Usage in projects/<name>/project.cmake:
#   set(PROJECT_LIBS hal bsp utility)
#
# sdk_register_library(<name> <subdir> TARGETS t1 [t2 ...] [DEPENDS d1 ...])
# sdk_require_library(<name>)   — idempotent, resolves transitive deps

set(_SDK_AVAILABLE_LIBS "")

# ---------------------------------------------------------------------------
# sdk_register_library(<name> <subdir> TARGETS t1 [t2 ...] [DEPENDS d1 ...])
# ---------------------------------------------------------------------------
macro(sdk_register_library _name _subdir)
    cmake_parse_arguments(_REG "" "" "TARGETS;DEPENDS" ${ARGN})
    set(_SDK_LIB_${_name}_DIR     "${_subdir}")
    set(_SDK_LIB_${_name}_TARGETS "${_REG_TARGETS}")
    set(_SDK_LIB_${_name}_DEPENDS "${_REG_DEPENDS}")
    set(_SDK_LIB_${_name}_ADDED   FALSE)
    list(APPEND _SDK_AVAILABLE_LIBS "${_name}")
endmacro()

# ---------------------------------------------------------------------------
# sdk_require_library(<name>)
# ---------------------------------------------------------------------------
macro(sdk_require_library _name)
    if(NOT DEFINED _SDK_LIB_${_name}_DIR)
        message(FATAL_ERROR "Unknown library '${_name}'. Available: ${_SDK_AVAILABLE_LIBS}")
    endif()
    if(NOT _SDK_LIB_${_name}_ADDED)
        foreach(_dep ${_SDK_LIB_${_name}_DEPENDS})
            sdk_require_library(${_dep})
        endforeach()
        add_subdirectory(
            "${_SDK_LIB_${_name}_DIR}"
            "${CMAKE_BINARY_DIR}/_libs/${_name}"
        )
        set(_SDK_LIB_${_name}_ADDED TRUE)
    endif()
endmacro()

# ===========================================================================
# Library registry — add new components here
# ===========================================================================

# --- HAL (chip-specific: SoC headers, LL drivers, HAL drivers) ----------
sdk_register_library(hal
    "${CMAKE_SOURCE_DIR}/components/hal"
    TARGETS hal
)

# --- BSP (board-level drivers: LCD, touch, sensors) --------------------
sdk_register_library(bsp
    "${CMAKE_SOURCE_DIR}/components/bsp"
    TARGETS bsp
    DEPENDS hal picolibc
)

# --- Utilities (log, ring buffer, bit-bang, CRC, etc.) -----------------
sdk_register_library(utility
    "${CMAKE_SOURCE_DIR}/components/utils"
    TARGETS utility
    DEPENDS hal
)

# --- LithoUI (C++17 embedded UI framework) -------------------------------
sdk_register_library(lithoui
    "${CMAKE_SOURCE_DIR}/components/lithoui"
    TARGETS litho
    DEPENDS hal picolibc
)

# --- C library (picolibc + compiler-rt) --------------------------------
sdk_register_library(picolibc
    "${CMAKE_SOURCE_DIR}/libc"
    TARGETS picolibc
)

# --- Middleware (FreeRTOS / LVGL / BLE — add as integrated) ------------
# sdk_register_library(freertos
#     "${CMAKE_SOURCE_DIR}/third_party/freertos"
#     TARGETS freertos_kernel
#     DEPENDS hal picolibc
# )
#
# sdk_register_library(lvgl
#     "${CMAKE_SOURCE_DIR}/third_party/lvgl"
#     TARGETS lvgl
#     DEPENDS hal
# )

# --- Algorithms (pedometer, heart-rate filter, raise-to-wake) ----------
# sdk_register_library(algorithms
#     "${CMAKE_SOURCE_DIR}/components/algorithms"
#     TARGETS algorithms
# )
