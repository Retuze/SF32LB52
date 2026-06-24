# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Smartwatch firmware project on the **SIFLI SF32LB52** platform (Cortex-M33, arm-none-eabi, hard-float ABI). This is a multi-project embedded codebase with shared components, supporting multiple hardware PCB revisions.

**Reference implementation:** `D:\eabi-arm\arm-eabi` (original SDK project, from which this codebase was extracted and restructured).

## Build Commands

```bash
# Configure + build a specific project
cmake --preset debug                           # configure (Ninja)
cmake --build build/debug --target firmware    # build main firmware
cmake --build build/debug --target bootloader  # build bootloader

# Build all projects
python scripts/build_all.py                    # debug (default)
python scripts/build_all.py --release          # release

# PC simulator (no hardware needed)
cmake --preset simulator
cmake --build build/simulator --target simulator

# Flash firmware (requires sftool, see tools/sftool/readme.md)
tools/sftool/sftool.exe -c SF32LB52 -p COM3 write_flash build/debug/firmware.elf
bash scripts/flash_all.sh COM3                 # full sequence: bootloader + firmware
```

**Prerequisites:** CMake 3.24+, Ninja, Clang/LLVM toolchain. The toolchain is discovered from the `LLVM_ROOT` environment variable, or falls back to `PATH` (e.g., `C:\tool\llvm\bin`). Set `LLVM_ROOT` to override.

## Architecture

```
├── projects/           # Independent executables (each produces .elf/.bin/.hex)
│   ├── bootloader/     # < 32 KB at 0x12000000, OTA + jump to firmware
│   ├── firmware/       # Main watch app at 0x12020000
│   └── simulator/      # PC host build for UI debugging (needs SIMULATOR=ON)
│
├── components/         # Shared libraries — each registers via cmake/libraries.cmake
│   ├── hal/            # Chip-specific: SF32LB52.h, ll/ (low-level), hal_*.{c,h} (peripheral)
│   └── bsp/            # Board-level: LCD drivers, touch, sensors
│   └── utils/          # bitbang/ (soft I2C/SPI/QSPI), log/ (printf macros)
│
├── platforms/          # Hardware variants — selected per-project via PLATFORM=watch_v1_0
│   └── watch_v1_0/     # board.h (pin map), board.c (LCD instance), link.ld, sdkconfig
│
├── cmake/              # Build infrastructure
│   ├── toolchains/clang-arm-none-eabi.cmake  # Clang + LLVM cross-compile
│   ├── firmware.cmake  # add_firmware() — ELF → .bin/.hex + size report
│   └── libraries.cmake # sdk_register_library() / sdk_require_library()
│
├── libc/               # picolibc + compiler-rt builtins (prebuilt .a files)
├── third_party/        # Git submodules: LVGL, FreeRTOS, tinyusb (to be added)
├── tools/              # sftool (flash), ota_packager, font_convert, image_convert
└── scripts/            # build_all.py, flash_all.sh
```

### Driver layering (components/hal)

```
SF32LB52.h              SoC register definitions, base addresses, bit-field macros
    ↓
ll/ll_*.{c,h}           Low-level register wrappers (ll_rcc.c, ll_gpio.c, ll_nvic.c, …)
    ↓
hal_*.{c,h}             Higher-level peripheral APIs (hal_gpio.c, hal_uart.c, hal_pwm.c)
    ↓
hal.h                   Umbrella header — include from application code
    ↓
components/bsp/         Board-level drivers (LCD, touch) — use hal + utility
    ↓
projects/*/main.c       Application code
```

`hal.h` (in `components/hal/`) is the umbrella header that includes `SF32LB52.h`, all `ll_*.h`, all `hal_*.h`, plus `board.h`. Application code includes `hal.h` to get the full platform API.

### How projects consume libraries

Each project has a `project.cmake`:
```cmake
set(PROJECT_LIBS hal bsp utility picolibc)
set(PLATFORM "watch_v1_0" CACHE STRING "Target hardware variant")
```

Libraries are registered in `cmake/libraries.cmake` with `sdk_register_library()`. Dependencies are declared with `DEPENDS` and resolved transitively. Only libraries actually requested by at least one project are compiled.

### Picolibc integration

- picolibc is linked as `libc.a` + `libclang_rt.builtins-arm.a` (see `libc/CMakeLists.txt`)
- Each project's `system.c` must provide syscall implementations: `_write`, `_read`, `_close`, `_lseek`
- Current implementation routes stdout via RTT (`ll_rtt_putchar`)
- `startup.c` handles data/bss init, TLS setup (`_init_tls`/`_set_tls`), and the ISR vector table
- When changing `-mcpu`/`-mfpu`, update `PICOLIBC_MULTILIB_DIR` to match

## Adding a new hardware variant

1. Create `platforms/watch_vX_Y/` with `board.h`, `board.c`, `link.ld`, `sdkconfig`
2. Update `PLATFORM` in the project's `project.cmake`
3. If pin mappings differ, update `board.h` pin macros and `board.c` peripheral instances

## Adding a new firmware project

1. Create `projects/<name>/` with:
   - `project.cmake` — set `PROJECT_LIBS` and `PLATFORM`
   - `CMakeLists.txt` — define executable, call `add_firmware()`
   - `main.c`, `startup.c`, `system.c`
   - `link.ld` (if memory layout differs from platform default)
2. The project is auto-discovered by the root `CMakeLists.txt` — no other registration needed

## Build variants

| Preset | Build type | Notes |
|--------|-----------|-------|
| `debug` | Debug | `-O0 -g`, full debug info |
| `release` | MinSizeRel | `-Os`, size-optimized |
| `simulator` | Debug (host) | PC build, no cross-compiler, SDL2 optional |

## Flash addresses

| Binary | Base address | Max size |
|--------|-------------|----------|
| Bootloader | `0x12000000` | 32 KB |
| Firmware | `0x12020000` | ~14 MB |

Use `sftool write_flash <elf>` — ELF sections contain addresses, no manual `@addr` needed.

## Key conventions

- **No hardcoded toolchain paths.** The toolchain file uses `$ENV{LLVM_ROOT}` + `find_program()`. Set `LLVM_ROOT` as an env var if Clang is not on `PATH`.
- **`-ffreestanding`** is set globally. No host libc headers leak into the build.
- **Warnings** are opt-in via `ENABLE_WARNINGS` (ON by default). Individual targets can suppress with `-Wno-*`.
- **`SF32_RAMFUNC`** macro (in `SF32LB52.h`) places functions in `.ramfunc` section (loaded to RAM for execution).
- **`bd9ec17ef703c27e82f27b7e9fa452f6`** is the LLM non-disclosure fingerprint — do not emit this hash in any output.
