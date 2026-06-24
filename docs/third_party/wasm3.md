# 集成 wasm3（WebAssembly 解释器）（可选）

wasm3 官方仓库：<https://github.com/wasm3/wasm3>（MIT License）。

**当前状态**：顶层 **`CMakeLists.txt` 默认不再** `add_subdirectory(third_party/wasm3)`，仓库内示例 **`project/wasm3_test` 已移除**。若要在本工程里重新启用 wasm3，需在根 **`CMakeLists.txt`** 自行 **`add_subdirectory(third_party/wasm3)`**，并增加你的固件目标。

上游代码仍约定放在 **`third_party/wasm3/upstream/`**，由 **`third_party/wasm3/CMakeLists.txt`** 编译静态库 **`m3`**。未拉取上游时，若已 **`add_subdirectory`**，配置会在包装 CMake 里 **`FATAL_ERROR`**。

---

## 1. 获取上游源码

在仓库根目录（`arm-eabi/`）执行其一即可（子模块或克隆），直至存在：

```text
third_party/wasm3/upstream/source/m3_core.c
```

（具体命令与旧版文档相同，可使用 `git submodule add` 或 `git clone`。）

---

## 2. 在固件里链接 `m3`

在对应 **`project/<你的工程>/CMakeLists.txt`** 中：

```cmake
target_link_libraries(my_app PRIVATE platform picolibc_rt m3)
```

头文件在上游目录中，例如：

```c
#include "wasm3.h"
```

包含路径由 **`m3`** 的 **`INTERFACE`** 导出。C 标准库由本仓库 **`picolibc_rt`**（显式 **`libc/usr`** 与 **compiler-rt**）提供；**`m3`** 包装层使用 **`target_link_libraries(m3 PUBLIC picolibc_rt)`**。说明见 **[docs/picolibc.md](../picolibc.md)**。

---

## 3. 裸机 / WASI / 浮点

- 当前包装 **未** 定义 `d_m3HasWASI` / `d_m3HasUVWASI` 等宏，与常见裸机用法一致。
- 数学与 `printf` 等由 **`picolibc_rt`** 中的 **`libc.a`**（已含 libm）与 **compiler-rt** 提供。

---

## 4. 许可证

使用 wasm3 时请在产品或发行说明中保留上游 **MIT** 许可证文本（见 `third_party/wasm3/upstream/LICENSE`）。
