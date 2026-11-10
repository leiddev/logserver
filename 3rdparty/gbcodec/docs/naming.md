# 命名约定

本文档说明 `gbcodec` 的名字、目录、宏、函数与变量的命名规则。历史实现使用
`unicode_utility`、`UnicodeUtility` 以及十个无前缀的全局函数，这些命名在
链接期容易冲突，也不便于检索，因此 1.0 重新制定了下面的规则。

## 库与包

| 名称 | 用途 |
| --- | --- |
| `gbcodec` | 库名、CMake 项目名、CMake 目标名、pkg-config 包名 |
| `gbcodec::gbcodec` | C 库的 CMake 别名，供 `find_package` / `add_subdirectory` 使用 |
| `gbcodec_cpp` / `gbcodec::gbcodec_cpp` | C++ 封装的接口库（header-only，C++11 起） |
| `<gbcodec/...>` | 公开头文件的包含路径 |

选择 `gbcodec` 的理由：

- 短（7 个字符），在 `#include`、`find_package` 与链接命令行中都不占宽度。
- 由 `gb`（GB2312）+ `codec`（编解码器）组成，领域含义明确，不会与
  `unicode`、`utf8` 之类的通用词撞名。
- 全部小写，无下划线，符合 C/C++ 库的常见习惯；在大小写不敏感的文件系统上
  也不会出现两种写法。

## 文件

- 公开头文件放在 `include/gbcodec/`，私有实现头文件放在 `src/detail/`。
- 文件名一律 `snake_case`，与模块名对应：`utf8.h`、`utf16.h`、`gb2312.h`、
  `convert.h`、`status.h`、`export.h`、`version.h`、`string_view.hpp`。
- 实现文件与头文件同名：`src/utf8.c` 实现 `include/gbcodec/utf8.h`。
- C++ 封装使用 `.hpp` 后缀（`gbcodec.hpp`），提醒使用者这是 C++ 头文件。
- 头文件保护宏为 `GBCODEC_<文件名>_H`，例如 `GBCODEC_UTF8_H`。不使用
  `#pragma once`，因为保护宏在复制头文件、生成文档等场景下更稳定。

## 宏

- **C 层面导出的常量**使用 `GBC_` 前缀，例如 `GBC_OK`、`GBC_E_UNMAPPABLE`。
- **构建配置与版本相关的宏**使用 `GBCODEC_` 前缀，例如 `GBCODEC_SHARED`、
  `GBCODEC_BUILDING_LIBRARY`、`GBCODEC_VERSION_MAJOR`，以及
  `<gbcodec/string_view.hpp>` 中的能力探测宏 `GBCODEC_USE_STD_STRING_VIEW`
  （为 1 时表示 C++17 下直接使用 `std::string_view`）。
- 只影响库自身编译的宏（`GBCODEC_BUILDING_LIBRARY`）保持 `PRIVATE`，不污染
  使用者的编译选项。
- 宏全部大写，单词之间用下划线分隔。

## C 函数

统一格式：

```text
gbc_<领域>_<动作>[_<变体>]
```

- `gbc_` 前缀保证在链接期不会与其它库冲突（历史接口 `UTF8ToUnicode` 等没有
  前缀，一旦被两个库同时提供就会链接失败）。
- `<领域>` 是 `utf8`、`utf16`、`gb2312`、`status` 之一，也可能是领域组合
  （`utf8_to_gb2312`）。
- `<动作>` 只有三种含义：
  - `decode`：字节流 → 码点；
  - `encode`：码点 → 字节流；
  - 领域之间的转换使用 `a_to_b` 形式。
- 变体后缀：
  - `_capacity`：只计算所需字节数，不写缓冲区；
  - `_replace`：无法映射的字符按调用方给出的码点替换（`0` 表示删除）；
  - `_z`：接收以 NUL 结尾的字符串（历史 C 风格接口）。
  后缀可以叠加，顺序固定为 `_replace_z`。

示例：

| 函数 | 含义 |
| --- | --- |
| `gbc_utf8_decode` | 从一个 UTF-8 字节序列解码出一个码点 |
| `gbc_utf8_encode` | 把一个码点编码为 UTF-8 |
| `gbc_gb2312_decode` | 从一个 GB2312 字符解码出一个码点 |
| `gbc_utf8_to_gb2312` | 显式长度的 UTF-8 → GB2312 转换 |
| `gbc_utf8_to_gb2312_z` | NUL 结尾的 UTF-8 → GB2312 转换 |
| `gbc_utf8_to_gb2312_capacity` | 查询转换结果长度 |
| `gbc_utf8_to_gb2312_replace` | 无法映射的字符按给定码点替换后转换 |
| `gbc_gb2312_to_utf8_replace_z` | 同上，接收 NUL 结尾的 GB2312 字符串 |

返回值的形状只有三种，不允许出现第四种：

1. 可能失败的操作用 `gbc_status`（`decode` 与整串转换）；
2. 不会失败、但长度可变的操作用 `size_t`（`encode`，返回 0 表示码点不可表示）；
3. 不涉及数据的操作用 `const char *`（`gbc_status_string`）。

## 类型

- 公开类型加 `gbc_` 前缀：`gbc_status`。
- 结构体 typedef 不带 `_t` 后缀，避免与 POSIX 保留命名空间冲突；
  内部结构为 `gbc_table_entry`。
- 定宽整数使用 `<stdint.h>` 的 `uint8_t` / `uint16_t` / `uint32_t`，
  长度与容量使用 `size_t`。不使用 `int`、`long`、`unsigned long` 表示数据，
  因为它们在 LP64 / LLP64 下宽度不同（历史实现中的 `unsigned long` 就是
  32/64 位行为不一致的根源）。

## 变量与参数

- 全部 `snake_case`，不使用匈牙利命名法。
- 输出参数统一加 `out_` 前缀：`out_code_point`、`out_consumed`、`out_length`、
  `out_replaced`。
- 带单位的参数在名字里带上单位或语义：`src_length`、`dst_capacity`、
  `src_bytes`、`encoded`。容量参数一律指**包含结尾 NUL 的总字节数**。
- 局部变量尽量短但可读：`lead`、`trail`、`code_point`、`consumed`、`written`。
- 布尔量用 `is_` 前缀，例如内部函数 `gbc_is_continuation`。

## C++ 命名空间与接口

- 命名空间为 `gbcodec`，不再使用 `UnicodeUtility` 这类首字母大写的旧名字。
- C++ 接口在 `namespace gbcodec` 下，函数与 C 层同类含义时使用
  `snake_case`：`to_gb2312`、`from_gb2312`、`try_to_gb2312`。
- 表示「可能失败」的两个版本成对出现：抛异常版本与 `try_` 前缀版本。
- 异常类型为 `gbcodec::error`，由 `std::runtime_error` 派生，并暴露
  `status()` 返回 `gbc_status`。
- 字符串参数类型统一为 `gbcodec::string_view`：C++17 下它是 `std::string_view`
  的别名，C++11 下是等价的只读视图类，调用方无需区分两种标准。
- 头文件形式（header-only）发布，不引入额外的 ABI。

## 与历史命名的对照

| 历史命名 | 现行命名 | 说明 |
| --- | --- | --- |
| `unicode_utility`（库/目标） | `gbcodec` | 与领域一致，避免与 Unicode 工具混淆 |
| `UnicodeUtility`（命名空间） | `gbcodec` | 小写、无缩写歧义 |
| `UTF8ToUnicode` | `gbc_utf8_decode` | 带前缀，输入输出方向由名字表达 |
| `UnicodeToUTF8` | `gbc_utf8_encode` | 同上 |
| `UTF16ToGB2312` | `gbc_gb2312_encode` | 先把 UTF-16 解码为码点，再编码 |
| `UTF8ToGB2312Str` | `gbc_utf8_to_gb2312` / `_z` | `Str` 后缀无法表达长度语义，改为计数版 + `_z` 版 |
| `utf16_to_gb2312_table.h` | `src/detail/gb2312_table.h` | 码表属于实现细节，不进入公开头文件 |
| `MAX_UTF16_SIZE` | 无 | 该常量描述的是实现细节，已删除 |
