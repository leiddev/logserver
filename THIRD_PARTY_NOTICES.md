# Third-Party Notices

LogServer 在 `3rdparty/` 目录下附带以下第三方库。这些库按原样随本仓库分发，未作修改。
每个库的完整许可证文本位于其目录内，相关文件路径见下表。

## asio 1.30.2

- 位置：`3rdparty/asio/`
- 版权：Copyright (c) 2003-2024 Christopher M. Kohlhoff
- 许可证：Boost Software License 1.0
- 许可证文本：`3rdparty/asio/LICENSE_1_0.txt`

用于 TCP、UDP 与 SSE 服务器的异步 I/O。

## cmdline

- 位置：`3rdparty/cmdline/`
- 版权：Copyright (c) 2009 Hideyuki Tanaka
- 许可证：BSD 3-Clause
- 许可证文本：`3rdparty/cmdline/LICENSE`

用于解析命令行参数。

## WebSocket++ 0.8.2

- 位置：`3rdparty/websocketpp/`
- 版权：Copyright (c) 2014 Peter Thorson
- 许可证：BSD 3-Clause
- 许可证文本：`3rdparty/websocketpp/COPYING`

用于 `WEBSERVER_TYPE=WEBSOCKET` 构建下的 WebSocket 服务。

## gbcodec 1.2.1

- 位置：`3rdparty/gbcodec/`
- 版权：Copyright (c) 2016-2026 leid
- 许可证：ISC
- 许可证文本：`3rdparty/gbcodec/LICENSE`

用于 WebSocket 与 SSE 推送时把 GB2312 日志转换为 UTF-8。支持有损转换：无法映射的
字符由调用者指定码点替代，不会静默丢字节。
