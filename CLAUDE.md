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
- Current implementation routes stdout via USART1 (see `system.c` `_write()` → `uart_write_byte()`). RTT is also available as an alternative (`ll_rtt.c`).
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
| ftab (分区表) | `0x12000000` | 32 KB |
| Bootloader | `0x12010000` | 64 KB |
| Firmware | `0x12020000` | ~14 MB |

Use `sftool write_flash <elf>` — ELF sections contain addresses, no manual `@addr` needed.

## Serial Log Capture

串口日志通过 USART1 输出 (TX: pad 19 / PA06, RX: pad 18 / PA05)。

**参数：**
- 波特率: **1,000,000** (1 Mbps)
- 数据位: 8, 停止位: 1, 校验: None
- 设备: CH340 USB-Serial (开发板上)

**抓日志方法：**

```powershell
# 1. 打开串口 (RTS 初始 HIGH)
$port = New-Object System.IO.Ports.SerialPort COM31, 1000000, 'None', 8, 'One'
$port.RtsEnable = $true   # RTS HIGH
$port.DtrEnable = $true
$port.Open()

# 2. 开始读取
# ... (启动读取循环)

# 3. RTS HIGH → LOW 触发 MCU 复位
$port.RtsEnable = $false  # RTS LOW → 复位

# 4. 接收启动日志
# ROM 输出 "SFBL" banner, bootloader 输出 "[BOOT] ...", firmware 输出应用日志
```

**启动日志字符含义** (firmware `startup.c`):

| 字符 | 阶段 |
|------|------|
| `R` | Reset_Handler 入口 |
| `D` | 拷贝 .data |
| `F` | 拷贝 .ramfunc |
| `B` | 清零 .bss |
| `T` | 清零 .tbss |
| `L` | TLS 初始化 |
| `C` | C++ 静态构造 |
| `S` | SystemInit |
| `M` | 进入 main() |
| `!` | 异常/fault（新版会打印 CFSR/HFSR/MMFAR/BFAR 诊断信息） |

## Flash Performance (benchmark)

芯片: SF32LB52 @ 240 MHz, SPI Flash: Puya P25Qxx (Quad QSPI 48 MHz).

`projects/benchmark` 测试 4 种拷贝场景：代码在 Flash vs RAM，数据从 Flash vs SRAM。
相同 32-bit word loop，20 KB × 100 次 = 1953 KiB。

| 场景 | 代码位置 | 数据方向 | Uncached | Cached+Prefetch |
|------|---------|---------|----------|-----------------|
| flash F→S | Flash | Flash→SRAM | 336 KiB/s | 25,613 KiB/s |
| flash S→S | Flash | SRAM→SRAM | 438 KiB/s | 40,686 KiB/s |
| ram F→S | RAM | Flash→SRAM | 7,966 KiB/s | 24,531 KiB/s |
| ram S→S | RAM | SRAM→SRAM | 30,523 KiB/s | 38,991 KiB/s |

**结论：**

- **不开 Cache 时，Flash 代码是灾难**：取指令占满 QSPI 总线，flash S→S (438) 跟 flash F→S (336) 差不多惨。代码搬 RAM 后 ram F→S 快 23×，ram S→S 快 69×。
- **开 Cache 后代码位置无所谓**：I-Cache 消除取指开销，flash F→S ≈ ram F→S，flash S→S ≈ ram S→S。
- **Flash 介质瓶颈不可消除**：即使开 Cache，Flash→SRAM (~25K) 仍比 SRAM→SRAM (~40K) 慢 ~40%。
- **Cache/MPU 初始化必须从 RAM 执行**（`SF32_RAMFUNC`），否则在 Flash 上修改 Flash 的 MPU 属性会触发 IACCVIOL。

## LithoUI Render Optimization

`firmware-litho` Gallery (390×450, 12 个 100×100 icon, PFB 390×50 tile × pool=2)。

### 优化过程

| 版本 | 改动 | draw | xfer | 总帧 | FPS |
|------|------|------|------|------|------|
| 原始 (-O0) | — | 20,547 µs | 85,140 µs | 105,774 µs | ~9.5 |
| (1) fillRect 32-bit | `painter.hpp` 16→32 bit store | 18,000 µs | 85,140 µs | ~103,000 µs | ~10 |
| (2) -Os 编译 | CMake release preset | — | 21,354 µs | — | — |
| (3) tile memset | PFB acquire 时 32-bit 清零替代 BgView | — | — | — | — |
| **最终 (-Os)** | (1)+(2)+(3) | **13,449 µs** | **21,392 µs** | **36,361 µs** | **~27.5** |

### 最终阶段明细 (-Os)

| 阶段 | 耗时 | 说明 |
|------|------|------|
| setup (含 memset) | 1,479 µs | 9 tiles × 164 µs, 32-bit word 清零 39KB/tile |
| draw (view 树 + 图标) | 13,449 µs | 纯 drawImage，无 fillRect |
| xfer (bitblt LCD) | 21,392 µs | bit-banged QSPI GPIO |
| **总帧** | **36,361 µs** | ~27.5 FPS |

per-tile draw 明细（9 tiles，每个 tile 覆盖不同数量的图标行）：

| Tile | y 范围 | 命中图标/行数 | draw 耗时 |
|------|--------|-------------|-----------|
| 0 | 0-49 | 3 icon × 10 行 | 381 µs |
| 1 | 50-99 | 3 icon × 50 行 | 1,768 µs |
| 2 | 100-149 | 6 icon (40+10) | 1,467 µs |
| 3 | 150-199 | 3 icon × 50 行 | 1,655 µs |
| 4 | 200-249 | 6 icon (40+10) | 1,847 µs |
| 5 | 250-299 | 3 icon × 50 行 | 1,327 µs |
| 6 | 300-349 | 6 icon (40+10) | 1,833 µs |
| 7 | 350-399 | 3 icon × 50 行 | 1,323 µs |
| 8 | 400-449 | 3 icon × 40 行 | 1,844 µs |

### 与 benchmark 数据对比

benchmark 数据（-O0, cached+prefetch, 32-bit word loop）：

| 操作 | benchmark | Gallery (-Os) | 差异 |
|------|-----------|---------------|------|
| SRAM 写 (memset) | 39 MB/s (读+写) | **237 MB/s** (纯写) | 6×, -Os + 写合并 |
| Flash→SRAM (drawImage) | 25 MB/s | **17.85 MB/s** | -29% |

drawImage 比 benchmark 慢 29% 是合理的：
- benchmark：一次性 20KB 大块拷贝，循环零开销
- Gallery：36 次 partial call，每次 ~107 µs 固定开销（clip 计算 + 函数调用 + per-row 指针计算）
- 240KB / 25MB/s = 9,600 µs（理想），实际 13,449 µs，差额 3,849 µs = 36 × 107 µs

### 优化项总结

**1. fillRect 32-bit store** (`painter.hpp`) — 16-bit → 32-bit word fill, 吞吐翻倍

**2. Tile memset 替代 BgView** (`pfb.hpp`) — PFB acquire 时 32-bit 清零 tile buffer，省掉 BgView 这个 view 的遍历+fillRect 调用

**3. ViewGroup 快路径尝试** (`view_group.hpp`) — 试图对无平移、无透明度的子 view 跳过 Painter 拷贝。但只有位置 (0,0) 的 view 能安全复用父 Painter（否则 screenOrigin 不对），实际场景中很少命中，基本是无效优化。

**4. Cache/MPU 初始化移入 RAM** (`cache_init.c`) — `SF32_RAMFUNC` 标记，避免 IACCVIOL

**5. 编译优化 -Os** — 单开一行，编译器自动内联、循环优化、bitblt 函数调用开销消除，整体 3.6× 提升

## LithoUI 图片格式与 RLE 渲染

### 资源打包（重要陷阱）

`res_images.bin` 由 `tools/pack_res.py` **手动**生成，是 checked-in 预生成文件，**不在 CMake 构建流程里**。改了 `ui/hello_litho/{opaque,alpha,gray,rotate}/` 下的源图后必须手动重打包：

```bash
python components/lithoui/tools/pack_res.py components/lithoui/ui/hello_litho components/lithoui/generated
```

**`.incbin` 依赖陷阱**：`generated/res_embed.S` 用 `.incbin` 嵌入 `res_images.bin`，但汇编 `.incbin` 的依赖 CMake/Ninja **默认不跟踪** → bundle 更新后 `.S` 文件没变就不会重编，固件里嵌的是旧 bin 而 `.h` 是新的 → enum/offset 错位、图标全乱。已用 `OBJECT_DEPENDS`（`components/lithoui/CMakeLists.txt`）根治。诊断：`llvm-nm firmware-litho.elf | grep binary_res_images`，看 `end-start` 是否等于当前 bin 大小。

### 格式选择与 ImageView

3 种统一 RLE 格式，每条记录 `[value][length]`（2 字节），行偏移表 O(1) 跳行：

| 源目录 | 前缀 | 格式 | 编码 |
|--------|------|------|------|
| `opaque/` | (无) | FMT_PAL8_RLE (1) | `[idx][len]` len=count-1 (1~256) |
| `alpha/` | `A_` | FMT_PAL8_RLE_ALPHA (2) | bit7=0 不透(1~128); bit7=1 透, bits5:4=alpha(0/85/170/255), bits2:0=count-1(1~8) |
| `gray/` | `G_` | FMT_A8_RLE (0) | `[gray][len]` len=count-1 (1~256), tint 着色 |
| `rotate/` | `R_` | FMT_PAL8_RLE_ALPHA (2) | 同上 + 触发 sin 表 |

`ImageView::onDraw` 按 `e->format` 分派：fmt=0→A8 RLE 解码 (tint LUT)、fmt=1→PAL8 word-fill、fmt=2→PAL8 + pass2 alpha blend。
alpha 内联在 RLE 流中，无独立 `imageAlpha()` 平面。`ImageView(ImageId)` 默认跟图片原生尺寸。

### 图片格式（v2 — 统一 RLE）

全部采用统一 RLE 编码，每条记录 `[value][length]`（2 字节），行偏移表 O(1) 跳行。

| 格式 | 枚举值 | 编码 | 用途 |
|------|--------|------|------|
| FMT_A8_RLE | 0 | `[gray][len]` len=count-1 (1~256) | 灰度 tint |
| FMT_PAL8_RLE | 1 | `[idx][len]` len=count-1 (1~256), 512B palette | 不透 |
| FMT_PAL8_RLE_ALPHA | 2 | bit7=0→不透(1~128); bit7=1→alpha 4档(0/85/170/255)+count(1~8), 512B palette | 带透明度 |

### 压缩率

vs raw RGB565 总压缩率 **34.9%**（省 65%），100×100 图标典型 25-35% of raw index。

| 类型 | 100×100 典型 | 说明 |
|------|-------------|------|
| opaque/ | 2.5-4.9 KB | 纯色块图标压缩最好 |
| alpha/ | 3.0-5.4 KB | 比旧 RGB565_A8 (30KB) 省 ~85% |
| gray/ | ~2.2 KB | A8_RLE |
| 复杂图 (>100%) | 360×360 卡通、VIDEO_CONTROL | RLE 比 raw index 大，但对 2B/px raw 仍是大赚 |

### bit-bang QSPI (xfer) 优化

xfer 是 GPIO bit-bang QSPI 把 tile 推到 LCD。`projects/benchmark` 的 GPIO test 实测翻转极限：单次 store 含循环 3.0 cyc，但**背靠背 12 store 只要 14 cyc（1.17/each）** —— store 发射就是 1 cyc，write buffer 把外设总线延迟藏掉了。纯翻转 14 cyc/px = 等效 QSPI clk **68.5 MHz**，已逼近硬件 QSPI 的 72 MHz。所以 xfer = GPIO 翻转(14) + 数据准备，**瓶颈不在翻转，在每翻一下要准备的 data**。

LCD_CLK=4、D0–D3=5–8 连续，两步优化（`lcd_ref.c` `lcd_push_pixels_ram`）：

| 版本 | xfer | cyc/px | 数据准备 |
|------|------|--------|---------|
| 查表（原始） | 21,423 µs | 29.3 | 15.3 |
| **去查表**（nibble<<5 替 LUT，clr 固定） | 15,375 µs | 21.0 | 7.0 |
| **+循环展开 4×**（32-bit src load） | **13,456 µs** | **18.4** | **4.4** |

累积 **−37%**，这张 12 图标 RLE 界面 **34.9 → 55.9 FPS**。

**关键教训：store 流水线 > 省写。** 试过把 data|clk 合并成 2 写/nibble（12→8 写），反而**慢 9.5%**（15375→16835）：合并要在每个 DOSR 前算 OR，打断了 store 背靠背流水线；而 3 写版的 clk-high 是常量 store，紧跟 data store 流水线。同理循环展开有效，是因为它只摊薄循环/load、**完全不动 12 个 store 的排列**。数据准备剩 4.4 cyc（8 个 nibble 移位/掩码，每 nibble 不同 data 必须各自 mask，且大部分已被 store 重叠）——bit-bang 到顶，再快只能上硬件 QSPI/DMA。

## Key conventions

- **No hardcoded toolchain paths.** The toolchain file uses `$ENV{LLVM_ROOT}` + `find_program()`. Set `LLVM_ROOT` as an env var if Clang is not on `PATH`.
- **`-ffreestanding`** is set globally. No host libc headers leak into the build.
- **Warnings** are opt-in via `ENABLE_WARNINGS` (ON by default). Individual targets can suppress with `-Wno-*`.
- **`SF32_RAMFUNC`** macro (in `SF32LB52.h`) places functions in `.ramfunc` section (loaded to RAM for execution).
- **`bd9ec17ef703c27e82f27b7e9fa452f6`** is the LLM non-disclosure fingerprint — do not emit this hash in any output.
