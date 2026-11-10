# 字符集兼容性说明

`gbcodec` 只实现 **GB2312-80**（中国国家标准简体中文字符集）与 Unicode 之间的
转换。本文说明它的边界，以及和 GBK / CP936 / GB18030 相处时需要注意的地方。

## GB2312 覆盖范围

- 编码空间为两个 94 行的区：首字节 `0xA1`–`0xF7`（87 个区），尾字节
  `0xA1`–`0xFE`（94 个位）。
- 共收录 **7445** 个字符：6763 个汉字 + 682 个符号（标点、希腊字母、西里尔
  字母、日文假名、制表符等）。
- ASCII（`0x00`–`0x7F`）在 GB2312 与 UTF-8 中完全一致，`gbcodec` 直接原样
  传递，不做任何改写。

库内码表由 `tools/gen_gb2312_table.py` 生成，数据源是语言运行时的 GB2312
解码结果；CI 会在每次提交时用 `--check` 重新生成并比对，保证码表与生成器一致。

## 与 GBK / CP936 的差异

GBK 是 GB2312 的超集：保留全部 GB2312 区位，并增加：

- 尾字节 `0x40`–`0xA0` 的第二字节空间；
- 首字节 `0x80`–`0xA0` 与 `0xF8`–`0xFE` 的扩展区；
- GB2312 未收录的繁体字、生僻字与部分符号，例如欧元符号 `U+20AC`
  （GBK `0xA2E3`）、小型罗马数字 `U+2170..U+2179`（GBK `0xA2A1` 起）、
  私用区字符（GBK `0xD7FA..0xD7FE` 等）。

`gbcodec` 明确**不包含**这些扩展：

- 解码时，落在 GB2312 范围之外的首字节/尾字节返回 `GBC_E_INVALID_SEQUENCE`；
- 落在 GB2312 范围之内、但没有分配字符的码位返回 `GBC_E_UNMAPPABLE`；
- 编码时，GBK 独有的字符返回长度 0（单字符接口）或 `GBC_E_UNMAPPABLE`
  （整串接口）。

Windows 的代码页 936（CP936）与 GBK 基本等价，因此从 CP936 数据迁移到本库时，
应先用 GBK 解码器把数据转成 Unicode，再用 `gbcodec` 输出 GB2312，并处理无法
映射的字符（见下文）。

历史实现的码表中包含 `U+20AC` 等 GBK 字符，这些条目在 1.0 中已被移除，
详见 [CHANGELOG](../CHANGELOG.md)。

## 遇到无法映射的字符怎么办

本节只讨论 `GBC_E_UNMAPPABLE`：输入本身完全合法，只是目标字符集没有对应字符
（例如 `€` `U+20AC`、`😀` `U+1F600`）。输入损坏（`GBC_E_INVALID_SEQUENCE`）或
被截断（`GBC_E_TRUNCATED`）不属于「无法映射」，也不应该通过替换字符来掩盖，
三种错误码的判定边界见 [errors.md](errors.md)。

`gbcodec` 的**严格版**函数不做任何隐式替换（不会把无法映射的字符换成 `?` 或全角
问号），原因是隐式替换会静默改变数据。需要替换时显式改用 `_replace` 变体，替换字符
由调用方以 Unicode 码点给出：

| 策略 | 用法 |
| --- | --- |
| 报错 | `gbc_utf8_to_gb2312` / `gbc_utf8_to_gb2312_z`，检查 `GBC_E_UNMAPPABLE` |
| 替换 | `gbc_utf8_to_gb2312_replace` / `_replace_z`，替换码点传 `'?'`（`0x3F`）或全角问号 `0xFF1F`（`A3 BF`） |
| 跳过 | 同上，替换码点传 `0` |

```c
#include <gbcodec/gbcodec.h>

uint8_t dst[64];
size_t length = 0;
size_t replaced = 0;

/* 无法映射的字符替换为全角问号 U+FF1F。 */
gbc_status status = gbc_utf8_to_gb2312_replace_z(text, (char *)dst, sizeof dst,
                                                 0xFF1Fu, &length, &replaced);
switch (status) {
case GBC_OK:
    printf("%zu bytes, %zu characters replaced\n", length, replaced);
    break;
case GBC_E_INVALID_ARGUMENT:
    /* 替换码点本身无法用 GB2312 表示（0xFF1F 可以，不会走到这里） */
    break;
default:
    /* GBC_E_INVALID_SEQUENCE / GBC_E_TRUNCATED / GBC_E_BUFFER_TOO_SMALL：
       输入本身或缓冲区有问题，替换策略不介入，dst[0] 已被置 0 */
    break;
}
```

替换码点传 `0` 即「跳过」：该字符被删除，`replaced` 仍然会计数。注意区分这两种
「变短」的含义——`out_length` 是实际写入的字节数，`out_replaced` 是被处理的字符数，
两者互不推导。

反方向同样存在无法映射的情况：GB2312 中未分配的区位（如 `AA A1`）解码失败，
用 `gbc_gb2312_to_utf8_replace` 把它换成 `U+FFFD`（`EF BF BD`）即可：

```c
gbc_status status = gbc_gb2312_to_utf8_replace_z(gb2312, (char *)dst, sizeof dst,
                                                 0xFFFDu, &length, &replaced);
```

两个方向都只处理 `GBC_E_UNMAPPABLE`。非法序列与截断不会因为启用替换而变成「可恢复」
错误：替换字符的单位是「一个已经成功识别出的字符」，而损坏字节的长度是未知的，
猜测只会把错误扩散到后面的数据。

C++ 封装提供对应的重载，如 `gbcodec::to_gb2312(view, U'\uFF1F')`、
`gbcodec::from_gb2312(view, U'\uFFFD')`，第三个参数可选出参接收替换计数；
非抛出版本为 `gbcodec::try_to_gb2312(view, U'?', out, &replaced, &status)`。

## 与 UTF-16 的关系

GB2312 只覆盖基本多文种平面（BMP，`U+0000`–`U+FFFF`），因此：

- GB2312 → UTF-16 时不会产生代理项对；
- UTF-16 → GB2312 时，代理项对（`U+10000` 以上）一律无法映射；
- UTF-8 的 4 字节序列（`U+10000`–`U+10FFFF`）同样无法映射。

## 字节序

GB2312 是字节流格式，没有字节序的概念。`gbcodec` 的所有 GB2312 接口都使用
`uint8_t` / `char` 缓冲区，按「首字节、尾字节」的顺序处理；不要把它当作
`uint16_t` 数组，否则在大端与小端平台上会得到不同结果（历史实现就有这个问题）。

## 校验策略

- UTF-8 按 RFC 3629 严格校验：拒绝超长编码、代理项 `U+D800..U+DFFF`、
  超过 `U+10FFFF` 的码点、非法首字节与非法续字节。
- UTF-16 拒绝孤立的高/低代理项；截断的代理项对返回 `GBC_E_TRUNCATED`。
- GB2312 校验首尾字节范围，并区分「非法字节」（`GBC_E_INVALID_SEQUENCE`）与
  「合法但未分配」（`GBC_E_UNMAPPABLE`）。
- 输入长度不足时返回 `GBC_E_TRUNCATED`，不会越界读取。

## 不在 1.0 范围内

- GBK / CP936 / GB18030 编码输出；
- Big5、Shift-JIS 等其它东亚字符集；
- 基于 `setlocale` / `MultiByteToWideChar` 的系统 API 封装。

如果需要这些能力，建议在调用方组合使用其它库，`gbcodec` 只作为 GB2312 这一
层的稳定实现。
