# 日志服务器程序

[![CI](https://github.com/leiddev/logserver/actions/workflows/ci.yml/badge.svg)](https://github.com/leiddev/logserver/actions/workflows/ci.yml)

日志服务器程序，用于接收各个应用程序通过 UDP 发送来的日志，将接收到的日志信息保存成文件，同时通过 TCP 对外提供日志服务。日志服务器还具备 Web 前端界面，方便用户通过浏览器查看实时日志信息。

## 开发要求

本程序支持在 Linux（x86、ARM）、Windows 环境下编译运行，支持 Linux x86 和 ARM 间交叉编译。建议在 Linux 下使用 gcc 及 g++ 6.3.0 或以上版本编译，在 Windows 下使用 Microsoft Visual Studio 2022 Community（社区版）编译。建议先在 Windows 下编译调试好后，再到 Linux 下编译部署应用。**注意任何对本代码库的修改不得破坏跨平台特性**，在源代码中善用 `#ifdef __unix__` 和 `#ifdef _WIN32`，引入的第三方库也必须具备跨平台特性。代码需保持 C++11 兼容。

## 主要功能

本程序主要实现了以下功能：

- 可设置接收日志的 UDP 端口号；
- 可设置对外提供日志的 TCP 服务端口号；
- 抓取到的日志信息会保存在 `/tmp/log/` 目录里面（Windows 下为 `.\log\`），仅当该目录为临时文件系统 `tmpfs` 时落盘，避免在嵌入式设备上写 flash（Windows 下无此限制，始终落盘）；
- 日志文件按照 1MB 大小自动切分，归档文件以本地日期时间命名，例如 `logserver_20261109_143005_123.txt`；文件后缀可在 `txt`（默认）与 `log` 之间选择；
- 可设置最大保存文件数量，当前文件与归档文件的总数超过该值后，删除最旧的归档文件；
- 可设置是否显示接收到的日志信息；
- 具备 Web 前端界面，可配置 Web 服务器端口号；
- Web 页面提供「关于」弹窗，显示软件名称、版本、当前传输模式（SSE 或 WebSocket）、版权与许可证；
- **WebSocket 服务器**：通过 WebSocket 协议向前端实时推送日志更新，支持多客户端并发连接；
- **SSE 服务器**：通过 Server-Sent Events（SSE）协议向前端实时推送日志更新，支持多客户端并发连接。

## 命令行参数

```
usage: ./LogServer [options] ...
options:
  -u, --udp_port     UDP port number (int [=11000])
  -t, --tcp_port     TCP port number (int [=11000])
  -s, --max_files    Maximum number of files (int [=100])
  -x, --log_suffix   Log file suffix, txt or log (string [=txt])
      --disp_msg     display receive message
  -w, --web_port     Web port number (int [=11001])
  -?, --help         print this message
```

**日志文件说明：**

- 日志文件后缀由 `-x/--log_suffix` 决定，可取 `txt`（默认）或 `log`，只接受这两个值；
- 正在写入的文件固定为 `logserver.txt`（使用 `-x log` 时为 `logserver.log`），可用 `tail -f` 持续查看；
- 当前文件超过 1MB 时，先按本地日期时间（精确到毫秒）改名为归档文件，例如 `logserver_20261109_143005_123.txt`，再重新创建当前文件；
- 若同一毫秒内发生多次切分，归档文件名会追加三位序号以避免覆盖，例如 `logserver_20261109_143005_123_001.txt`；
- `-s/--max_files` 限制目录中日志文件的总数（当前文件 + 归档文件）。达到上限后每次切分都会删除最旧的归档文件；设为 `0` 表示不写日志文件，此时也不会创建日志目录；
- 清理只针对当前后缀的归档文件：切换后缀后，另一种后缀的旧文件不会被自动删除；
- 运行期间若当前文件被外部删除、改名或所在目录被删除，程序会自动重建并继续写入（最多延迟 32 条日志或 1 秒）；若文件被原地截断（如 logrotate 的 `copytruncate`），程序会重新同步大小继续追加，不会误触发切分。该检测仅在 Linux 等 POSIX 平台生效，Windows 下已打开的文件无法被删除；
- 程序自身的运行日志带 `[LogServer] ` 前缀（`APP_NAME` 加方括号），与接收到的日志保存在同一个文件里；接收到的日志按原始内容写入，不添加前缀、时间戳或换行；
- 接收到的日志是否同时输出到控制台由 `--disp_msg` 控制；
- 文件日志未能启用时（日志目录不在 tmpfs 上、路径被同名文件占用、目录无法创建等），启动时会向 stderr 输出以 `file logging disabled:` 开头的原因说明；`-s 0` 属于主动关闭，不会有提示。

**Web 服务器可选配置：**

通过 CMake 选项 `WEBSERVER_TYPE` 选择 Web 服务器类型，无需修改源码：

| `WEBSERVER_TYPE` | 说明 |
|---|------|
| `NONE` | 禁用 Web 服务器，此时没有 `-w/--web_port` 参数 |
| `WEBSOCKET` | 使用 WebSocket 协议 |
| `SSE` | 使用 Server-Sent Events 协议（**默认**） |

```
cmake -S . -B build -DWEBSERVER_TYPE=WEBSOCKET
```

**WebSocket 服务器使用说明：**

- WebSocket 服务器与 Web 服务器共用相同的端口
- 访问根路径 `/` 显示日志查看页面
- WebSocket 连接端点为根路径 `/`，用于接收实时日志推送
- 日志流是单向的：客户端发来的数据帧会被忽略，只处理 ping/pong/close 等控制帧
- 支持多客户端并发连接，断开连接后自动清理资源
- 与 TCP、SSE 服务器一致，监听 IPv4 地址

**SSE 服务器使用说明：**

- SSE 服务器与 Web 服务器共用相同的端口
- 访问根路径 `/` 显示日志查看页面
- SSE 连接端点为 `/sse`，用于接收实时日志推送
- 支持多客户端并发连接，断开连接后自动清理资源
- 与 TCP、WebSocket 服务器一致，监听 IPv4 地址

## 使用的第三方库

（Linux 下均为静态链接使用，各库许可证详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)）

- [cmdline](3rdparty/cmdline)：命令行参数解析库（BSD 3-Clause）；
- [gbcodec](3rdparty/gbcodec)：GB2312 与 UTF-8 转换库（ISC）；
- [asio](3rdparty/asio)：异步 IO 库（Boost Software License 1.0）；
- [WebSocket++](3rdparty/websocketpp)：WebSocket 协议库，依赖 asio（BSD 3-Clause）。

## 目录结构

```
app/            主程序源码
  inc/            头文件
  src/            源文件
  inc/html/       内嵌到可执行文件的 Web 页面
tests/          测试
3rdparty/       附带的第三方库
config/         systemd 服务配置示例
```

## 配置文件说明

**（1）主配置文件**

（无）

## 程序编译方式

使用 CMake 生成编译配置，CMake 版本至少应该为 3.15。

### 预编译版本

[Releases](https://github.com/leiddev/logserver/releases) 页面提供 Windows x64 与 Linux x64 的预编译包，内含可执行文件、README、LICENSE、第三方声明、CHANGELOG 与 `SHA256SUMS`。

- **Linux x64**：在 glibc 2.28 环境构建并静态链接 libstdc++，可在麒麟 V10（glibc 2.31）、Debian 11 等较旧发行版上运行；
- **Windows x64**：MSVC 2022 构建。

ARM 平台（含 Hi3516）没有预编译包，请按下文自行交叉编译。

### 通用方式

```
cmake -S . -B build
cmake --build build
```

构建产物位于 `build/out/app/`。

### Hi3516 ARM 平台

```
mkdir build;cd build
cmake -D CMAKE_TOOLCHAIN_FILE="../3516.toolchain.cmake" ..
make
```

### PC Linux 平台

```
mkdir build;cd build
cmake -D CMAKE_TOOLCHAIN_FILE="../x64.toolchain.cmake" ..
make
```

### Windows 平台

使用 Microsoft Visual Studio 2022 Community（社区版）打开包含主 CMakeLists.txt 的文件夹。

请参阅：
[Visual Studio 中的 CMake 项目](https://learn.microsoft.com/zh-cn/cpp/build/cmake-projects-in-visual-studio?view=msvc-170)

## 测试

构建时默认启用测试（`BUILD_TESTING=ON`），需要 Python 3 才会注册端到端测试：

```
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
cd build && ctest --output-on-failure
```

- `app_log`：日志格式化、原始写入保真、`initFile` 守卫、切分与保留策略、重启追加等单元测试。写文件的用例需要 tmpfs，Linux 上使用 `/dev/shm`，Windows 上使用 `%TEMP%`。
- `server_e2e`：启动真实进程，校验 UDP→TCP 转发、SSE 推送、HTTP 页面、`SIGINT` 正常退出，以及文件日志是否遵循 tmpfs 门槛。
- `web_frontend`：SSE/WebSocket 前端的稳定性与健壮性——停滞的浏览器被剔除且不阻塞转发、事件完整有序、中途断开、畸形请求、连接抖动。
- `transport_stability`：UDP 生产接口与 TCP 消费接口的专项测试——1000 条带序号数据报的顺序/完整性/丢包率、65507 字节最大报文、空报文、多生产者、字节级保真；TCP 多消费者扇出、慢消费者被剔除、中途断开、洪泛下连接抖动、分片写入的回显、带活跃连接退出。三个 `WEBSERVER_TYPE` 变体都会运行。

## 许可证

本项目使用 [MIT License](LICENSE)。

## 跨平台开发注意事项

**（1）换行符**

仓库通过 [.gitattributes](.gitattributes) 将所有文本文件统一为 `LF`，无论在哪一个平台检出，行尾都是一致的。新增文件请保持 `LF`；IDE 与编辑器配置也可由 [.editorconfig](.editorconfig) 自动读取。

如果你仍想在本机强制保留行尾，可执行：

```
git config --global core.autocrlf false
```

**（2）文件编码**

Linux 下使用的文件编码为 UTF-8，而 Windows 下使用的文件编码默认为 GBK。当源代码文件中没有非 ASCII 码字符时，Linux 下的编译器和 Windows 下的编译器都能正常工作。但是一旦文件中使用了非 ASCII 码字符（如中文），由于中文在不同编码下的二进制值不同，某一平台下的编译器将会出现大量错误提示，而另一平台下的编译器却正常。

为了避免这种情况，**建议不要在源代码文件中使用中文等非 ASCII 字符，注释尽量使用英文**。实在无法避免在源代码中使用中文，可手动将源文件转换为 **UTF-8（With BOM）** 格式，这样全平台的编译器都能正确识别源文件的编码格式。

**（3）构建缓存**

`out/`、`build/` 等构建目录已在 `.gitignore` 中排除，请勿提交。
