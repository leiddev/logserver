# 错误码与接口变体

本文说明 `gbcodec` 返回的错误码如何判定，以及同一个转换为什么提供
`gbc_xxx_to_yyy`、`..._capacity`、`..._z` 三种调用形态。

## 一、三个「数据类」错误码

关键区别只有一句话：**`GBC_E_INVALID_SEQUENCE` 是「这些字节永远不合法」，
`GBC_E_TRUNCATED` 是「字节还不够」，`GBC_E_UNMAPPABLE` 是「字节合法但没有对应的
字符」。**

| 错误码 | 判定依据 | 补上更多字节能否变合法 | 责任方 |
| --- | --- | --- | --- |
| `GBC_E_INVALID_SEQUENCE` | 违反编码规则：非法首字节、非法续字节、超出范围、超长编码、代理项 | 不能 | 数据损坏、编码器错误 |
| `GBC_E_TRUNCATED` | 首字节声明的长度大于剩余字节数 | 能（补全后可能合法） | 输入被截断、长度参数给错 |
| `GBC_E_UNMAPPABLE` | 序列完全合法，但目标字符集没有这个字符 | 不能 | 字符集能力限制 |

另外三个错误码与数据内容无关，属于调用方式问题：

| 错误码 | 触发条件 |
| --- | --- |
| `GBC_OK` | 成功，值为 0，可以直接做真假判断 |
| `GBC_E_INVALID_ARGUMENT` | 传入了必需的 `NULL` 指针（如 `src`、`dst`，或单字符接口的输出参数） |
| `GBC_E_BUFFER_TOO_SMALL` | 目标缓冲区容量（**含结尾 NUL**）不足以容纳结果 |

`gbc_status_string()` 可以把任意状态码转成固定的英文描述，便于写日志。

## 二、UTF-8

单字符接口：`gbc_utf8_decode(src, src_length, &code_point, &consumed)`。

| 输入字节 | 返回 | 原因 |
| --- | --- | --- |
| `41` | `GBC_OK` | ASCII `A`，消耗 1 字节 |
| `E4 B8 AD` | `GBC_OK` | U+4E2D「中」 |
| `F0 9F 98 80` | `GBC_OK` | U+1F600 😀（UTF-8 层合法） |
| `80` | `GBC_E_INVALID_SEQUENCE` | 孤立的续字节，不能作为首字节 |
| `C0 80` | `GBC_E_INVALID_SEQUENCE` | 超长编码（本应写作 `00`） |
| `ED A0 80` | `GBC_E_INVALID_SEQUENCE` | 代理项 U+D800，UTF-8 中不允许 |
| `F4 90 80 80` | `GBC_E_INVALID_SEQUENCE` | 大于 U+10FFFF |
| `F5 ..`、`FF` | `GBC_E_INVALID_SEQUENCE` | 首字节不在合法范围 |
| `E4 41 41` | `GBC_E_INVALID_SEQUENCE` | 第 2 字节不是续字节 |
| `E4` | `GBC_E_TRUNCATED` | 三字节序列只给了 1 字节 |
| `E4 B8` | `GBC_E_TRUNCATED` | 缺最后 1 字节 |
| `F0 9F 98` | `GBC_E_TRUNCATED` | 四字节序列只给了 3 字节 |

注意 `E4` 与 `E4 41` 的区别：前者是「不够」，后者是「给够了但内容非法」。

## 三、GB2312

单字符接口：`gbc_gb2312_decode(src, src_length, &code_point, &consumed)`。
合法空间是首字节 `0xA1`–`0xF7`、尾字节 `0xA1`–`0xFE`。

| 输入字节 | 返回 | 原因 |
| --- | --- | --- |
| `41` | `GBC_OK` | ASCII |
| `B0 A1` | `GBC_OK` | U+554A「啊」 |
| `A1 A1` | `GBC_OK` | U+3000 表意空格（已分配，不是「未分配」） |
| `81 A1` | `GBC_E_INVALID_SEQUENCE` | 首字节 `0x81` 不在 `0xA1`–`0xF7`（属 GBK 区） |
| `F8 A1` | `GBC_E_INVALID_SEQUENCE` | 首字节 `0xF8` 属 GBK 扩展区 |
| `A1 40` | `GBC_E_INVALID_SEQUENCE` | 尾字节 `0x40` 不在 `0xA1`–`0xFE` |
| `A1 FF` | `GBC_E_INVALID_SEQUENCE` | 尾字节 `0xFF` 超出 `0xFE` |
| `A1` | `GBC_E_TRUNCATED` | 只有首字节 |
| `AA A1` | **`GBC_E_UNMAPPABLE`** | 首尾字节都在合法范围内，但该区位没有分配字符 |

`AA A1` 与 `A1 A1` 的差别，正是 `GBC_E_UNMAPPABLE` 与 `GBC_OK` 的差别：前者是
「合法但没字」，后者是「合法且有字」。

## 四、UTF-16

单字符接口：`gbc_utf16_decode(src, src_length, &code_point, &consumed)`。

| 输入 | 返回 |
| --- | --- |
| `D83D DE00` | `GBC_OK`（U+1F600） |
| `DE00` | `GBC_E_INVALID_SEQUENCE`（孤立低代理项） |
| `D83D D83D` | `GBC_E_INVALID_SEQUENCE`（高代理后面跟的不是低代理） |
| `D83D` | `GBC_E_TRUNCATED`（高代理后面没有数据） |

## 五、整串转换中的「无法映射」

解码通过、但目标字符集没有对应字符时返回 `GBC_E_UNMAPPABLE`：

| 方向 | 输入 | 返回 |
| --- | --- | --- |
| UTF-8 → GB2312 | `E2 82 AC`（€ U+20AC，GBK 独有） | `GBC_E_UNMAPPABLE` |
| UTF-8 → GB2312 | `F0 9F 98 80`（😀 U+1F600，超出 BMP） | `GBC_E_UNMAPPABLE` |
| UTF-8 → GB2312 | `EE 80 80`（U+E000，私用区） | `GBC_E_UNMAPPABLE` |
| GB2312 → UTF-8 | `AA A1`（未分配区位） | `GBC_E_UNMAPPABLE` |

前三个输入本身都是合法的 UTF-8，只是 GB2312 中没有这些字符。如果把这类情况报成
`GBC_E_INVALID_SEQUENCE`，调用方会误以为数据损坏，因此必须区分开。

`gbcodec` 的严格版函数不做隐式替换（不会自动写成 `?`）；需要替换时用
`gbc_utf8_to_gb2312_replace` / `gbc_gb2312_to_utf8_replace`，由调用方指定替换码点，
这两组函数**永不返回 `GBC_E_UNMAPPABLE`**。非法序列与截断在它们这里仍然是致命错误。
策略细节见 [compatibility.md](compatibility.md#遇到无法映射的字符怎么办)。

## 六、错误码判断示例

```c
#include <gbcodec/gbcodec.h>

int convert_or_explain(const char *utf8, size_t length, char *out, size_t capacity)
{
    size_t written = 0;
    const gbc_status status = gbc_utf8_to_gb2312((const uint8_t *)utf8, length,
                                                 (uint8_t *)out, capacity, &written);
    switch (status) {
    case GBC_OK:
        return 0;
    case GBC_E_TRUNCATED:
        /* 输入不完整：补齐后可以重试 */
        return 2;
    case GBC_E_INVALID_SEQUENCE:
        /* 数据损坏：重试没有意义，需要修正数据源 */
        return 3;
    case GBC_E_UNMAPPABLE:
        /* 数据没问题，只是 GB2312 装不下：替换为 '?' 或跳过 */
        return 4;
    case GBC_E_BUFFER_TOO_SMALL:
        /* 调用方的问题：先用 _capacity 查长度 */
        return 5;
    default:
        return 1;
    }
}
```

无论哪种失败，转换函数都不会留下半截数据：

- 目标缓冲区容量大于 0 时，`dst[0]` 被写成 NUL；
- `*out_length` 保持调用前的值不变；
- 不会写到 `dst_capacity` 之外。

## 七、空输入的特例

单字符接口需要「一个字符」，所以长度 0 属于「不够」：

```c
gbc_utf8_decode(src, 0, &cp, &consumed);   /* GBC_E_TRUNCATED */
gbc_gb2312_decode(src, 0, &cp, &consumed); /* GBC_E_TRUNCATED */
gbc_utf16_decode(src, 0, &cp, &consumed);  /* GBC_E_TRUNCATED */
```

整串接口中空串是合法的：

```c
uint8_t dst[8];
size_t length = 99;
gbc_utf8_to_gb2312(src, 0, dst, sizeof(dst), &length); /* GBC_OK, length == 0, dst[0] == 0 */
```

## 八、三种调用形态

以 UTF-8 → GB2312 为例，共有四个公开函数：

| 函数 | 输入形式 | 是否写数据 | 输出 |
| --- | --- | --- | --- |
| `gbc_utf8_to_gb2312` | 指针 + 长度 | 写 | `gbc_status` + 实际写入字节数 |
| `gbc_utf8_to_gb2312_capacity` | 指针 + 长度 | 不写 | `gbc_status` + 所需字节数 |
| `gbc_utf8_to_gb2312_z` | NUL 结尾字符串 | 写 | `gbc_status` + 实际写入字节数 |

`gbc_gb2312_to_utf8` 一侧完全对称。三种形态共享同一套错误码：对同一个输入会返回
相同的 `GBC_E_INVALID_SEQUENCE` / `GBC_E_TRUNCATED` / `GBC_E_UNMAPPABLE`，唯一
不同的是 `_capacity` 永远不会返回 `GBC_E_BUFFER_TOO_SMALL`。

### 8.1 计数版

```c
gbc_status gbc_utf8_to_gb2312(const uint8_t *src, size_t src_length,
                              uint8_t *dst, size_t dst_capacity,
                              size_t *out_length);
```

- `src_length` 是精确字节数：输入不需要 NUL 结尾，可以包含内嵌 `0x00`；
- `dst_capacity` 是目标缓冲区的总大小，**包含结尾 NUL 的位置**；
- 成功时写入结尾 NUL，`*out_length` 是不含 NUL 的字节数；
- `out_length` 可以传 `NULL`。

容量「含 NUL」这一点最容易记错：

```c
/* "中文" → 4 字节 GB2312 */
uint8_t tight[4];
size_t length = 0;
gbc_utf8_to_gb2312(src, 6, tight, 4, &length);   /* GBC_E_BUFFER_TOO_SMALL */

uint8_t ok[5];
gbc_utf8_to_gb2312(src, 6, ok, 5, &length);      /* GBC_OK, length == 4, ok[4] == 0 */
```

### 8.2 测量版 `_capacity`

```c
gbc_status gbc_utf8_to_gb2312_capacity(const uint8_t *src, size_t src_length,
                                       size_t *out_length);
```

完全执行一遍转换（含校验），但只统计长度、不写内存；返回的长度**不含 NUL**。

```c
size_t needed = 0;
if (gbc_utf8_to_gb2312_capacity((const uint8_t *)src, length, &needed) != GBC_OK) {
    return handle_error();
}

char *buffer = malloc(needed + 1);            /* 记得 +1 给结尾 NUL */
size_t written = 0;
if (gbc_utf8_to_gb2312((const uint8_t *)src, length, (uint8_t *)buffer, needed + 1,
                       &written) != GBC_OK) {
    free(buffer);
    return handle_error();
}
/* written == needed */
```

三个用途：

1. **一次分配、一次写入**，避免「先猜容量、失败再重来」；
2. **提前校验**：输入非法时在分配内存之前就能拿到错误码；
3. **估算输出规模**：例如判断转换结果是否超过某个阈值。

代价是遍历两遍输入（每遍 O(n)，查表为 O(1) / O(log n)）。若已知输出上界，
直接用计数版更快。

### 8.3 C 字符串版 `_z`

```c
gbc_status gbc_utf8_to_gb2312_z(const char *src, char *dst,
                                size_t dst_capacity, size_t *out_length);
```

`_z` 表示 zero-terminated：内部先 `strlen(src)`，再调用计数版，语义完全一致，
只是省去调用方自己算长度。

```c
char gb2312[64];
size_t length = 0;
if (gbc_utf8_to_gb2312_z("中文abc", gb2312, sizeof gb2312, &length) == GBC_OK) {
    /* gb2312 已 NUL 结尾，length == 7 */
}
```

不适合二进制数据：遇到内嵌 `0x00` 会被截断，此时应改用计数版：

```c
static const uint8_t data[] = {0x41, 0x00, 0x42};   /* "A\0B" */
uint8_t out[8];
size_t length = 0;
gbc_utf8_to_gb2312(data, sizeof(data), out, sizeof(out), &length); /* GBC_OK, length == 3 */
```

### 8.4 命名规律

```text
gbc_<源>_to_<目标>            计数版：显式长度，二进制安全
gbc_<源>_to_<目标>_capacity   只测量：不写内存，返回所需字节数（不含 NUL）
gbc_<源>_to_<目标>_z          C 字符串版：strlen 后调用计数版
```

## 九、对应的测试

本文中的示例基本都有对应的断言，可以直接查看：

- `tests/test_utf8.c`：非法序列、截断、编码边界；
- `tests/test_gb2312.c`：字节范围、未分配区位、全码表遍历；
- `tests/test_utf16.c`：代理项与截断；
- `tests/test_convert.c`：整串转换的容量矩阵、内嵌 NUL、`_z` 变体。
