# Changelog

本文件记录 LogServer 的所有重要变更，格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

版本号沿用项目既有的 `V0.x` 形式，并非严格遵循语义化版本；破坏性变更会在条目中显式标注。

## [Unreleased]

### Changed

- Web 前端改用一套克制的配色：一个中性面、一个强调色和一个危险色，按钮按层级区分而不是各自一种实心色。启动是页面上唯一的实心按钮，停止为红色描边，自动滚动开启时浅蓝底、关闭时中性灰，清空与保存为中性，关于为无边框文字按钮。
- Web 页面头部改为单行工具栏：标题与按钮分居左右，窄屏自动换行；状态文字前增加颜色圆点（绿=已连接，黄=连接中/重连中，红=错误），日志中的 info/error 行改用 CSS class 着色，不再逐行写内联 `style`。
- Web 页面标签页标题改为 `APP_NAME` 加「日志监控」，即 `LogServer 日志监控`。

### Added

- Web 页面新增「关于」按钮，弹窗显示软件名称、版本、传输模式（SSE 或 WebSocket）、版权与许可证；按 Esc 或点「关闭」即可退出。
- 页面里的名称与版本由 `html_page.cc` 在首次请求时从 `common.h` 的 `APP_NAME`、`APP_VERSION_STR` 填入，版本号仍只在 `common.h` 维护一处。

## [V0.8.10] - 2026-11-11

### Changed

- Web 前端页面合并为一份实现：布局、样式与日志渲染放进共用的 `log_page_head.html`，两种模式各自只保留连接层（`log_page_sse.html`、`log_page_ws.html`），`log_page_tail.html` 负责按钮绑定与收尾，`html_page.cc` 由相邻字符串字面量在编译期拼成一页。重连策略此前两边各写了一遍且行为不一致，现在只有一份；每个构建也只带自己那份连接代码。
- WebSocket 页面补上状态行，与 SSE 一致显示「连接中 / 已连接 / 重连中 / 已断开」；两种模式的页面标题统一为「日志监控」。
- `APP_NAME` 移入 `common.h`，`main.cc` 与 TCP 欢迎语改为引用它，不再各自硬编码。
- 程序自身运行日志的前缀由 `[APP] ` 改为 `[LogServer] `，即 `[` + `APP_NAME` + `]`；改名前后用前缀区分自身日志与接收日志的脚本需要同步调整。

### Fixed

- 用户手动断开后，已经排队的重连定时器仍会再次连上，两种模式都有此问题；现在手动断开后不再重连。

## [V0.8.9] - 2026-11-11

### Fixed

- TCP 消费端的每个连接此前有一条无界的待发队列：一个连上却不读数据的客户端，在日志洪泛下会让进程内存持续上涨。现在单个连接的待发字节超过 4 MB 即关闭该连接，与 SSE、WebSocket 的上限一致，其余客户端不受影响。
- UDP 收到 0 字节报文时被记为 `UDP receive error: Success`。空报文不是错误，现在静默忽略。
- UDP 停止后残留的接收回调仍会再次投递接收操作，在已关闭的套接字上产生错误，`stop()` 之后现在直接返回。

### Added

- `tests/transport_stability_test.py`：UDP 生产接口与 TCP 消费接口的专项测试。
  - UDP：1000 条带序号数据报的顺序、完整性与丢包率（≤1%），65507 字节最大报文，空报文，多生产者，含 NUL/CRLF/格式化字符的字节级保真。
  - TCP：3 个与 50 个并发消费者的扇出与顺序，慢消费者按上限被剔除且不阻塞 IO 线程，中途断开不影响他人，洪泛下的连接抖动，分片写入的回显只回到发送者，带活跃连接时的退出。
  - 三个 `WEBSERVER_TYPE` 变体都注册运行。

## [V0.8.8] - 2026-11-10

### Fixed

- WebSocket 模式下，一个不读数据的浏览器会让 websocketpp 的发送队列持续增长，进程内存随之上涨。现在广播时检查每个连接的待发字节数，超过 4 MB 即以 `policy_violation` 关闭该连接并移出订阅列表，与 SSE 侧的上限一致。
- WebSocket 的动作队列此前没有上限：处理线程落后时，每条日志都会在队列里留下一份拷贝。现在按条数（4096）与载荷字节（8 MB）双重设限，超出即丢弃新的日志行并计数，每丢弃 1024 条输出一次告警。订阅、退订与停止动作不受限制，避免订阅状态失步。
- SSE 的 HTTP 请求头没有长度上限也没有超时：只连接不发送、或一直不发送空行的客户端可以把缓冲区撑到耗尽内存，并长期占用连接。现在请求头限制为 8 KB，超出返回 `431 Request Header Fields Too Large` 并关闭连接；请求超过 10 秒未到达即关闭。
- SSE 读取请求出错时此前只记录日志而不关闭连接，连接会一直挂着；现在读取到空请求的错误一律关闭。
- 两种 Web 模式的 404 页面把请求路径原样拼进 HTML。现在先做 HTML 转义再输出。
- SSE 模式退出时 `SSE server stopped.` 会打印两次，现在只打印一次。

### Changed

- WebSocket 模式不再回显客户端发来的数据帧。日志流是单向的，查看页面也从不发送数据；此前的回显会把客户端可控内容写进服务端日志，并让一个客户端可以放大自己产生的流量。ping/pong/close 等控制帧不受影响。

### Added

- Web 前端测试补充：客户端数据帧被忽略且不影响连接、慢速 WebSocket 客户端被服务端剔除、SSE 请求头的长度上限与超时、404 页面转义。

## [V0.8.7] - 2026-11-10

### Fixed

- SSE 向浏览器推送时在 UDP 接收线程上做阻塞写：一个不读数据的浏览器把发送缓冲填满后，UDP 接收、TCP 转发与日志落盘全部停住，SIGINT 也无法处理。现在每个客户端有独立的发送队列，写入只在 SSE 自己的 io_context 线程进行；某个客户端累计积压超过 4 MB 视为掉线并被关闭，其余客户端不受影响。
- SSE 同一连接存在两条写路径（广播的阻塞写与心跳的异步写），心跳可能插进一条大日志中间，破坏事件分帧。现在所有写出都经过同一个队列。
- SSE 在响应头写出之前就把客户端加入推送列表，首个事件可能抢在 HTTP 头之前。现在只在 EventSource 请求确定后注册并排队。
- WebSocket 广播线程无锁读取订阅者集合，与处理线程的增删形成数据竞争（未定义行为，客户端上下线时触发）。订阅者集合现在只在处理线程访问。
- WebSocket 在 `run()` 未能监听（如 Web 端口被占用）时收到 SIGINT 会挂死：STOP 分支异常路径二次解锁连接锁且不退出处理循环，主线程 `join()` 永不返回。现在改用免异常重载并保证循环退出，重复调用 `stop()` 安全。
- WebSocket 的 `is_listening()` 从 UDP 线程读取 websocketpp 的内部状态，与工作线程的启停形成数据竞争。改为由服务器自己维护的原子标志。

### Added

- `tests/web_frontend_test.py`：SSE/WebSocket 的稳定性与健壮性用例——停滞消费者不阻塞转发、积压超限被剔除、事件完整有序、客户端中途断开、畸形请求、连接抖动。
- `tests/websocket_stop_test.cc`、`tests/websocket_broadcast_race_test.cc`：停止流程与广播竞态的回归用例，后者由 `-DLOG_SERVER_TSAN_TESTS=ON` 构建并由 CI 单独运行。

## [V0.8.6] - 2026-11-10

### Fixed

- SSE 模式此前把原始字节直接写入事件流，GB2312 日志在浏览器里显示为乱码（EventSource 一律按 UTF-8 解码）。现在与 WebSocket 一样先转成 UTF-8。
- WebSocket 遇到既非合法 UTF-8、也非合法 GB2312 的日志时，旧库返回空串，整条日志静默丢失。现在无法解码的字节替换为 U+FFFD，日志行不再消失。
- 旧库 `unicode_utility` 在有损转换失败时把 `-1` 存入无符号变量，32 位平台上越界检查回绕，`memcpy` 收到负长度：**单个畸形 UDP 报文即可使 32 位构建的进程崩溃**。替换库后该路径不再存在。
- SSE 响应头把结尾的 NUL 一并写出，首个事件字段变成 `"\0data"`，被 EventSource 忽略，第一次心跳丢失。
- SSE 与页面响应此前未声明字符集，现在 `Content-Type` 带 `charset=utf-8`。

### Changed

- 第三方库 `unicode_utility` 替换为 `gbcodec` 1.2.1，带错误码与调用者指定码点的有损转换接口。
- 构建最低 CMake 版本由 3.10 提升到 3.15（gbcodec 的要求）。
- 未指定 `CMAKE_BUILD_TYPE` 时默认使用 `Release`，此前是空值（等于无优化）。

## [V0.8.5] - 2026-11-10

### Added

- 启动时输出 Web 服务器类型与端口：`Web server: SSE mode on port N`、`Web server: WebSocket mode on port N`，未启用 Web 服务器时输出 `Web server: disabled`。

### Changed

- 源文件、头文件、测试与内嵌页面的注释统一为英文，每个文件补上说明用途的文件头注释；页面文案、日志内容与文档的中文保持不变。

## [V0.8.4] - 2026-11-10

### Fixed

- UDP 接收缓冲区此前固定为 1024 字节，超过该长度的日志报文会被内核截断且不报错，导致长日志（如打印配置 JSON）落盘时被切断，并因丢失结尾换行而与下一条日志粘在同一行。现在缓冲区扩展到 65536 字节，覆盖 IPv4 下 65507 字节的最大载荷。

## [V0.8.3] - 2026-11-09

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

[Unreleased]: https://github.com/leiddev/logserver/compare/v0.8.10...HEAD
[V0.8.10]: https://github.com/leiddev/logserver/releases/tag/v0.8.10
[V0.8.9]: https://github.com/leiddev/logserver/releases/tag/v0.8.9
[V0.8.8]: https://github.com/leiddev/logserver/releases/tag/v0.8.8
[V0.8.7]: https://github.com/leiddev/logserver/releases/tag/v0.8.7
[V0.8.6]: https://github.com/leiddev/logserver/releases/tag/v0.8.6
[V0.8.5]: https://github.com/leiddev/logserver/releases/tag/v0.8.5
[V0.8.4]: https://github.com/leiddev/logserver/releases/tag/v0.8.4
[V0.8.3]: https://github.com/leiddev/logserver/releases/tag/v0.8.3
[V0.8.2]: https://github.com/leiddev/logserver/releases/tag/v0.8.2
[V0.8.1]: https://github.com/leiddev/logserver/releases/tag/v0.8.1
[V0.8]: https://github.com/leiddev/logserver/releases/tag/v0.8
