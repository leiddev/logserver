# gbcodec

[![CI](https://github.com/leiddev/gbcodec/actions/workflows/ci.yml/badge.svg)](https://github.com/leiddev/gbcodec/actions/workflows/ci.yml)
[![License: ISC](https://img.shields.io/badge/license-ISC-blue.svg)](LICENSE)

`gbcodec` 是一个轻量的 C / C++ 库，用于 **GB2312** 与 **Unicode（UTF-8 / UTF-16）**
之间的相互转换。它不依赖任何第三方库，也不依赖操作系统的字符集 API，因此可以在
Windows 与 Linux 上得到完全一致的结果。

## 特性

- 纯 C99 实现，可选 C++ 封装（C++11 起，C++17 下自动使用 `std::string_view`），
  无第三方依赖。
- 转换结果只取决于码表，与操作系统区域设置（locale / code page）无关。
- 显式长度与容量参数，不使用全局状态，线程安全。
- 同时提供单字符（码点级）与整串（缓冲区级）转换接口。
- 完整的错误码：非法序列、截断、无法映射、缓冲区不足可分别判断。
- 严格校验 UTF-8（拒绝过长编码、代理区与超过 U+10FFFF 的值）。
- 内置 O(1) / O(log n) 查表，不使用线性扫描。
- 在 Windows（MSVC）与 Linux（GCC）的 x86 / x64 上持续集成，并覆盖共享库与
  AddressSanitizer / UndefinedBehaviorSanitizer 构建。

## 支持的平台

| 平台 | 架构 | 工具链 | 状态 |
| --- | --- | --- | --- |
| Windows | x64 | MSVC 2019+ | CI 覆盖 |
| Windows | x86 | MSVC 2019+ | CI 覆盖 |
| Linux | x64 | GCC 9+ / Clang 10+ | CI 覆盖 |
| Linux | x86 | GCC 9+（`-m32`） | CI 覆盖 |

> 目前只覆盖 GB2312（6763 个汉字 + 682 个符号）。GBK / GB18030 / CP936 不在 1.0 的
> 范围内，详见 [docs/compatibility.md](docs/compatibility.md)。

## 快速开始

C 语言：

```c
#include <gbcodec/gbcodec.h>

#include <stdio.h>

int main(void)
{
    char gb2312[64];
    size_t length = 0;

    /* 源文件需以 UTF-8 保存。 */
    const gbc_status status = gbc_utf8_to_gb2312_z("中文abc", gb2312, sizeof gb2312, &length);
    if (status != GBC_OK) {
        fprintf(stderr, "转换失败: %s\n", gbc_status_string(status));
        return 1;
    }

    printf("%zu 字节的 GB2312 数据\n", length);
    return 0;
}
```

C++（可选，C++11 起）：

```cpp
#include <gbcodec/gbcodec.hpp>

#include <string>

int main()
{
    const std::string gb2312 = gbcodec::to_gb2312("中文abc"); // 失败时抛出 gbcodec::error
    const std::string utf8 = gbcodec::from_gb2312(gb2312);
    return utf8.empty() ? 1 : 0;
}
```

## API 概览

| 头文件 | 内容 |
| --- | --- |
| `<gbcodec/gbcodec.h>` | 汇总头文件，包含下面的全部 C 接口 |
| `<gbcodec/utf8.h>` | `gbc_utf8_decode` / `gbc_utf8_encode` |
| `<gbcodec/utf16.h>` | `gbc_utf16_decode` / `gbc_utf16_encode` |
| `<gbcodec/gb2312.h>` | `gbc_gb2312_decode` / `gbc_gb2312_encode` |
| `<gbcodec/convert.h>` | `gbc_utf8_to_gb2312`、`gbc_gb2312_to_utf8` 及其 `_z` / `_capacity` / `_replace` 变体 |
| `<gbcodec/status.h>` | `gbc_status` 错误码与 `gbc_status_string` |
| `<gbcodec/gbcodec.hpp>` | C++ 封装 `gbcodec::to_gb2312` 等（头文件形式，C++11 起；C++17 下自动使用 `std::string_view`） |
| `<gbcodec/string_view.hpp>` | `gbcodec::string_view`：C++17 下是 `std::string_view` 的别名，C++11 下是等价的只读视图 |

约定：

- 所有函数的输入都是「显式长度」的字节序列，不依赖 NUL 结尾，允许出现内嵌 NUL。
- 整串转换函数会预留 1 字节存放结尾的 NUL，并在成功时写入结尾的 NUL。
- 失败时目标缓冲区首字节被写成 NUL，输出参数保持不变，绝不会写出缓冲区边界。
- GB2312 缓冲区按字节流顺序处理（先首字节后尾字节），与主机的字节序无关。

### 错误码

| 值 | 含义 |
| --- | --- |
| `GBC_OK` | 成功（值为 0） |
| `GBC_E_INVALID_ARGUMENT` | 参数非法：NULL 指针，或替换码点无法用目标编码表示 |
| `GBC_E_INVALID_SEQUENCE` | 输入不是合法的字节序列 |
| `GBC_E_TRUNCATED` | 输入在序列中间结束 |
| `GBC_E_UNMAPPABLE` | 字符在目标字符集中没有对应项 |
| `GBC_E_BUFFER_TOO_SMALL` | 目标缓冲区不足（含结尾 NUL） |

### 有损转换

`gbc_utf8_to_gb2312_replace` / `gbc_gb2312_to_utf8_replace`（以及 `_z` 变体）在字符
无法用目标编码表示时用调用者给出的码点替换它，而不是返回 `GBC_E_UNMAPPABLE`：

```c
uint8_t buffer[64];
size_t length = 0;
size_t replaced = 0;

/* 把 emoji 之类的字符换成 '?'；传入 0 表示直接删除该字符。 */
gbc_status status = gbc_utf8_to_gb2312_replace_z("中文😀abc", (char *)buffer, sizeof buffer,
                                                 '?', &length, &replaced);
```

- 替换码点是 Unicode 码点，由库按目标编码写入；无法用目标编码表示时返回
  `GBC_E_INVALID_ARGUMENT`。
- 只处理 `GBC_E_UNMAPPABLE`：非法序列和截断仍然整体失败，不会静默丢字节。
- 输出不会超过输入，因此 UTF-8 → GB2312 用 `src_length + 1` 字节、GB2312 → UTF-8 用
  `3 * src_length + 1` 字节就总是够用，不需要预先测量。

## 构建

需要 CMake 3.15 及以上版本，以及一个支持 C99 的 C 编译器（C++ 封装另需 C++11
及以上的编译器）。

```bash
# 配置 + 构建 + 测试
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows 上使用 Visual Studio 生成器时可以指定架构：

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

也可以直接使用预设：

```bash
cmake --preset linux-x64        # 或 windows-x64 / windows-x86 / linux-x86
cmake --build --preset linux-x64
ctest --preset linux-x64
```

可执行文件（以及 Windows 下的 DLL）统一输出到 `build/bin/<配置>/`。

### 构建选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `OFF` | 构建共享库；只导出标记为 `GBC_API` 的符号 |
| `GBCODEC_BUILD_CXX` | 顶层项目时 `ON` | 提供 C++ 封装（C++11 起）；作为子项目时默认关闭，避免强制依赖 C++ 编译器 |
| `GBCODEC_BUILD_EXAMPLES` | 顶层项目时 `ON` | 构建示例程序 |
| `GBCODEC_BUILD_TESTS` | 顶层项目时 `ON` | 构建单元测试 |
| `GBCODEC_WARNINGS_AS_ERRORS` | `OFF` | 把编译告警当作错误 |

## 安装与在其它项目中使用

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/gbcodec
cmake --build build --parallel
cmake --install build
```

安装内容：

- `include/gbcodec/`：公开头文件（含生成的 `version.h`）。
- `lib/libgbcodec.*`（共享构建时为 `bin/*.dll` 加 `lib/*.lib`）。
- `lib/cmake/gbcodec/`：CMake 包配置，支持 `find_package(gbcodec CONFIG)`。
- `lib/pkgconfig/gbcodec.pc`：pkg-config 元数据。

### CMake

```cmake
find_package(gbcodec CONFIG REQUIRED)

target_link_libraries(my_app PRIVATE gbcodec::gbcodec)      # C 接口
target_link_libraries(my_app PRIVATE gbcodec::gbcodec_cpp)  # 加上 C++ 封装
```

注意：`gbcodec::gbcodec_cpp` 只有在构建时开启 `GBCODEC_BUILD_CXX` 才会提供，
可通过 `gbcodec_CXX_AVAILABLE` 变量判断。

### 作为子目录嵌入

```cmake
add_subdirectory(third_party/gbcodec)

target_link_libraries(my_app PRIVATE gbcodec::gbcodec)
```

作为子项目时，`GBCODEC_BUILD_TESTS` / `GBCODEC_BUILD_EXAMPLES` /
`GBCODEC_BUILD_CXX` 默认关闭，不会给宿主项目引入额外负担。

### pkg-config

```bash
pkg-config --cflags --libs gbcodec
```

## 目录结构

```text
.
├─ include/gbcodec/  公开头文件（C 接口 + C++ 封装）
├─ src/              实现
│  └─ detail/        生成的 GB2312 码表（不对外公开）
├─ examples/         示例程序
├─ tests/            单元测试
├─ tools/            码表生成脚本
├─ docs/             设计、命名与兼容性文档
└─ .github/          CI 与协作模板
```

## 路线图

- [x] 建立仓库、导入基线、搭建 CI（Windows / Linux，x86 / x64）
- [x] 补齐测试并修复内存安全与正确性缺陷
- [x] 重建码表，改为 O(1) / O(log n) 查找
- [x] 重新设计并重命名公开 API（`gbcodec` / `gbc_` 前缀 / C++ 封装）
- [x] 打包、文档与 1.0.0 发布

## 文档

- [docs/naming.md](docs/naming.md)：库、命名空间、函数与变量的命名约定。
- [docs/errors.md](docs/errors.md)：错误码的判定边界（非法序列 / 截断 / 无法映射）
  以及 `_capacity` / `_z` 变体的用法。
- [docs/compatibility.md](docs/compatibility.md)：GB2312 的覆盖范围，以及与
  GBK / CP936 的差异和无法映射时的处理方式。
- [CHANGELOG.md](CHANGELOG.md)：版本变更与历史接口的迁移对照。

## 参与贡献

请先阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。提交缺陷时请附上平台、编译器版本与
最小复现用例。

## 许可证

[ISC](LICENSE)，Copyright (c) 2016-2026 leid。
