# Picolibc 与裸机链接

本工程**不使用 `CMAKE_SYSROOT` / `--sysroot`**。头文件与静态库路径按仓库 **`libc/`** 布局在 **`CMakeLists.txt`** 里配置（目标 **`picolibc_rt`**）。

**假定 picolibc 以 Meson `-Dposix-console=true` 构建**：stdin/stdout/stderr 由 **libc.a** 提供。对当前 **`blink` / `lcd_test`** 做链接实测，**仅需**在各自 **`project/*/system.c`** 实现 **`write` / `read` / `lseek` / `close`**（接 RTT）即可通过链接。

若使用 **`malloc` / `free`**：`libc.a` 里 **`malloc` 未定义引用 `sbrk`**，同时 **`libos_fallback_sbrk.c.o` 提供弱符号 `W sbrk`**（可用 **`llvm-nm libc.a | findstr sbrk`** 自证）。在各 project 的 **`system.c`** 实现强符号 **`sbrk`**（堆自 **`_end`** 增至 **`__StackLimit`**）即可覆盖弱实现，**不必**在链接脚本里定义 **`__heap_start` / `__heap_end`**。

若仍出现 **`_exit` / `kill` / `getpid` / `_fstat` / `_isatty`** 等 undefined，再按需补到对应 project 的 **`system.c`** 即可。

## 目录变量

| CMake 缓存 | 默认 | 含义 |
|------------|------|------|
| **`PICOLIBC_PREFIX`** | **`usr/local`** | 在 **`libc/`** 下的 picolibc 安装前缀（其下需有 **`include/`**、**`lib/`**） |
| **`PICOLIBC_MULTILIB_DIR`** | **`v8-m.main+fp`** | **`lib/<multilib>/libc.a`** 与 **`libc/builtin/<multilib>/`** 子目录名 |

默认完整路径示例：

- **`libc/usr/local/include`**
- **`libc/usr/local/lib/v8-m.main+fp/libc.a`**
- **`libc/builtin/v8-m.main+fp/libclang_rt.builtins-arm.a`**

若安装前缀仍是 **`libc/usr/...`**，可配置：**`-DPICOLIBC_PREFIX=usr`**。

## 链接顺序

**`picolibc_rt`**：**`libc.a` → `libclang_rt.builtins-arm.a`**。各 firmware target 自己的 **`system.c`** 提供 syscall 强符号。

## CPU/ABI

须与 **`cmake/toolchains/clang-arm-none-eabi.cmake`** 一致；更换 **`-mcpu` / `-mfpu`** 时请同步 **`PICOLIBC_MULTILIB_DIR`** 并放置对应 **`.a`**。
