# libc — picolibc + compiler-rt builtins
#
# Provides the `picolibc` INTERFACE target that brings in:
#   - picolibc headers (libc/usr/local/include)
#   - libc.a (picolibc, built with Meson -Dposix-console=true)
#   - libclang_rt.builtins-arm.a (compiler-rt, matching -mcpu/-mfpu)
#
# Syscalls (_write, _read, _sbrk, etc.) must be implemented in each
# project's system.c.  See docs/picolibc.md for details.
#
# Cached variables (override with -D):
#   PICOLIBC_PREFIX       — relative path under libc/ (default: usr/local)
#   PICOLIBC_MULTILIB_DIR — multilib subdir under lib/ and builtin/
#                            (default: v8-m.main+fp)

set(PICOLIBC_PREFIX "usr/local" CACHE STRING
    "picolibc install prefix relative to libc/ (must contain include/ and lib/)")
set(PICOLIBC_MULTILIB_DIR "v8-m.main+fp" CACHE STRING
    "picolibc + compiler-rt multilib subdirectory (matches -mcpu/-mfpu)")

set(LIBC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/${PICOLIBC_PREFIX}")

# ---- Validate paths ----------------------------------------------------
if(NOT EXISTS "${LIBC_ROOT}/include")
    message(FATAL_ERROR "picolibc headers not found at:\n  ${LIBC_ROOT}/include\n"
                        "Build picolibc with Meson or adjust PICOLIBC_PREFIX.")
endif()

set(_LIBC_A "${LIBC_ROOT}/lib/${PICOLIBC_MULTILIB_DIR}/libc.a")
if(NOT EXISTS "${_LIBC_A}")
    message(FATAL_ERROR "libc.a not found at:\n  ${_LIBC_A}")
endif()

set(_COMPILER_RT "${CMAKE_CURRENT_SOURCE_DIR}/builtin/${PICOLIBC_MULTILIB_DIR}/libclang_rt.builtins-arm.a")
if(NOT EXISTS "${_COMPILER_RT}")
    message(FATAL_ERROR "compiler-rt builtins not found at:\n  ${_COMPILER_RT}")
endif()

# ---- picolibc interface target -----------------------------------------
add_library(picolibc INTERFACE)
target_include_directories(picolibc SYSTEM INTERFACE "${LIBC_ROOT}/include")
target_link_libraries(picolibc INTERFACE
    "${_LIBC_A}"
    "${_COMPILER_RT}"
)
