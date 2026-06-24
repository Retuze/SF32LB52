# Toolchain file for SF32LB52 (Cortex-M33) — Clang + LLVM
#
# Discovers tools from LLVM_ROOT env var or PATH. No hardcoded paths.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARM_TARGET "arm-none-eabi" CACHE STRING "Cross compile target triple")
set(ARM_CPU    "cortex-m33"    CACHE STRING "Target CPU")
set(ARM_FPU    "fpv5-sp-d16"   CACHE STRING "Target FPU")
set(ARM_FLOAT  "hard"          CACHE STRING "Float ABI")

# ---- discover toolchain from environment --------------------------------
# LLVM_ROOT env var takes priority; falls back to PATH search.
set(LLVM_ROOT "$ENV{LLVM_ROOT}" CACHE PATH "LLVM install root (optional)")
if(LLVM_ROOT)
    file(TO_CMAKE_PATH "${LLVM_ROOT}" LLVM_ROOT)
    set(_LLVM_HINT "${LLVM_ROOT}" "${LLVM_ROOT}/bin")
endif()

find_program(CMAKE_C_COMPILER   NAMES clang      clang.exe      HINTS ${_LLVM_HINT} REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES clang++    clang++.exe    HINTS ${_LLVM_HINT} REQUIRED)
find_program(CMAKE_AR           NAMES llvm-ar     llvm-ar.exe    HINTS ${_LLVM_HINT} REQUIRED)
find_program(CMAKE_RANLIB       NAMES llvm-ranlib llvm-ranlib.exe HINTS ${_LLVM_HINT} REQUIRED)
find_program(CMAKE_OBJCOPY      NAMES llvm-objcopy llvm-objcopy.exe HINTS ${_LLVM_HINT} REQUIRED)
find_program(CMAKE_SIZE         NAMES llvm-size   llvm-size.exe  HINTS ${_LLVM_HINT} REQUIRED)
find_program(CMAKE_OBJDUMP      NAMES llvm-objdump llvm-objdump.exe HINTS ${_LLVM_HINT} REQUIRED)

set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}" CACHE FILEPATH "" FORCE)
set(CMAKE_C_COMPILER_TARGET   "${ARM_TARGET}")
set(CMAKE_CXX_COMPILER_TARGET "${ARM_TARGET}")
set(CMAKE_ASM_COMPILER_TARGET "${ARM_TARGET}")

# ---- arch + abi flags --------------------------------------------------
set(_ARCH_FLAGS "--target=${ARM_TARGET} -mcpu=${ARM_CPU} -mfpu=${ARM_FPU} -mfloat-abi=${ARM_FLOAT} -mthumb")
set(_C_FLAGS    "${_ARCH_FLAGS} -ffreestanding -fdata-sections -ffunction-sections -fno-builtin -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables")
set(_CXX_FLAGS  "${_ARCH_FLAGS} -ffreestanding -fdata-sections -ffunction-sections -fno-builtin -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables")

set(CMAKE_C_FLAGS_INIT          "${_C_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT        "${_CXX_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT        "${_ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_ARCH_FLAGS} -fuse-ld=lld")
