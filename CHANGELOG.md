# Changelog

本文件记录 LogServer 的所有重要变更，格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

版本号沿用项目既有的 `V0.x` 形式，并非严格遵循语义化版本；破坏性变更会在条目中显式标注。

## [Unreleased]

### Fixed

- 运行期间日志文件被外部删除、改名或所在目录被删除时，进程此前会继续写入不可见的 inode，目录里看不到任何文件，直到触发 1MB 切分才恢复；现在会检测到并在最多 32 条日志或 1 秒内重建文件（目录一并重建），向 stderr 留一行提示。
- 日志文件被原地截断（如 logrotate 的 `copytruncate`）时，大小计数会重新同步，不再因旧计数误判溢出而提前切分。

## [V0.8.2] - 2026-11-09

### Added

- 新增 `-x/--log_suffix` 参数，可在 `txt` 与 `log` 之间选择日志文件后缀，只接受这两个值，默认 `txt`。

### Changed

- 文件日志无法启用时输出明确原因，不再静默禁用：日志目录不在 tmpfs 上、路径被同名文件占用、目录无法创建或 `statfs` 失败，都会向 stderr 输出 `file logging disabled: ...`，并带上系统错误信息与文件系统类型；`-s 0` 仍保持静默。
- 日志文件后缀默认值由 `.log` 改为 `.txt`，需要 `.log` 时请显式指定 `-x log`。
- 归档清理只匹配当前后缀的文件，切换后缀不会删除另一种后缀的旧文件。

## [V0.8.1] - 2026-11-09

### Fixed

- WebSocket 变体在 Linux 上无法响应任何请求：`listen(port)` 默认绑定 IPv6 端点，IPv4 客户端被接受为 IPv4 映射连接后，HTTP 页面与 WebSocket 握手响应都写不出去，客户端一直挂起；现改为显式绑定 IPv4，与 TCP、SSE 服务器一致。
- `WEBSOCKET` 变体的日志查看页面恢复可用。

### Changed

- CI 中三个 `WEBSERVER_TYPE` 变体改为构建并运行完整测试，此前只做构建。

## [V0.8] - 2026-11-09

首个公开发布版本，以 MIT 许可证开源。

### Added

- 新增 `app_log` 模块，自带日志实现：控制台输出、`{}` 占位符格式化、按大小切分的文件归档。
- 归档文件以本地日期时间命名，例如 `logserver_20261109_143005_123.log`；同一毫秒内重复切分会追加三位序号。
- 新增 MIT `LICENSE` 与 `THIRD_PARTY_NOTICES.md`。
- 新增 `tests/`：`app_log` 单元测试与 `server_e2e` 端到端测试，通过 CTest 运行。
- 新增 GitHub Actions CI，在 Linux（gcc）与 Windows（MSVC）上构建并运行测试。
- 新增 `WEBSERVER_TYPE` CMake 选项（`NONE`/`WEBSOCKET`/`SSE`），无需修改源码即可选择 Web 服务器类型。
- 新增 `CONTRIBUTING.md`、`CHANGELOG.md`、`.editorconfig`、`.gitattributes`，统一换行符为 LF。

### Changed

- 移除 `bl` 与 `spdlog` 两个第三方依赖。
- `-s/--max_files` 语义变更：现在限制目录中日志文件的总数（当前文件 + 归档文件），此前为「当前文件 + max_files 个备份」。
- 接收到的日志按原始字节写入文件，不再经过格式化，也不添加前缀、时间戳或换行。
- 文件日志模式下，接收到的日志不再自动输出到控制台，控制台输出完全由 `--disp_msg` 控制。
- 当前日志文件由 `logserver.txt` 改为 `logserver.log`。
- `app_log` 编译为独立静态库 `applog`，供测试直接链接。
- README 重写，补充构建、测试、目录结构与许可证说明。

### Fixed

- `-s 0` 时不再创建日志目录。
- 文件写入失败时仅向 stderr 报告并降级为控制台输出，不再中断服务。

### Removed

- 移除 `app/inc/utility.hh` 与 `app/src/utility.cc`，其目录处理辅助函数并入 `app_log`。

### Known limitations

- `WEBSOCKET` 变体对普通 HTTP `GET /` 请求不作响应，日志查看页面无法通过浏览器打开。

## [V0.7] - 2026-09-21

### Added

- 新增 SSE 服务器，作为 WebSocket 之外的前端推送方案，默认启用。
- 前端支持批量渲染，提升大量日志下的刷新性能。
- README 补充 WebSocket 与 SSE 服务器文档。

### Changed

- 命令行参数 `-f` 改名为 `-s/--max_files`。
- `-s/--max_files` 允许设为 `0`，表示不写日志文件。
- Web 前端页面文件 `log_page.html` 改名为 `ws_log_page.html`。

### Fixed

- SSE 推送多行日志时，为每一行添加 `data:` 前缀。

## [V0.6] - 2025-02-17

### Added

- 新增 `unicode_utility` 库，WebSocket 推送遇到 `invalid_payload` 时将 GB2312 转换为 UTF-8。
- 显示 WebSocket 错误码，便于定位问题。

### Fixed

- 修正 WebSocket 服务器与日志页面的若干问题。

## [V0.5] - 2025-02-12

### Changed

- TCP 与 UDP 服务器改用 asio 实现。
- 优化 WebSocket 服务器的消息处理。

### Added

- 新增 `utility.cc` / `utility.hh`，提供目录创建与文件系统类型判断。

### Removed

- 移除 `tcp_server_client` 库及无用代码。

## [V0.4] - 2025-02-10

### Added

- 新增 Web 前端页面，可在浏览器中查看实时日志。
- 新增 `-w/--web_port` 参数，配置 Web 服务器端口。
- 新增 WebSocket 服务器，支持多客户端并发连接。
- 新增 `ENABLE_WEB_SERVER` 编译开关，用于选择 Web 服务器类型。

## [V0.3] - 2024-02-23

### Added

- 支持将日志文件保存到 tmpfs（`/tmp/log/`），避免在嵌入式设备上写 flash。
- 新增 `Log::setPattern`。

### Fixed

- 修正 TCP 发送超时问题。

## [V0.2 及更早] - 2023-09 ~ 2024-02

此阶段未标记版本号，以下按提交归纳，日期取自各提交时间。

### Added

- 初始版本：通过 UDP 接收日志，通过 TCP 对外提供日志服务。
- 命令行参数解析、日志级别设置。
- 按大小切分日志文件，并可限制保留的文件数量。
- 新增 `LogServer.service`，便于以 systemd 服务方式运行。
- 新增 `-DSPDLOG_COMPILED_LIB`，以编译库方式使用 spdlog。

[Unreleased]: https://github.com/leiddev/logserver/compare/v0.8.2...HEAD
[V0.8.2]: https://github.com/leiddev/logserver/releases/tag/v0.8.2
[V0.8.1]: https://github.com/leiddev/logserver/releases/tag/v0.8.1
[V0.8]: https://github.com/leiddev/logserver/releases/tag/v0.8
