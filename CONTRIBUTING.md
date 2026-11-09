# 贡献指南

感谢你愿意为 LogServer 做出贡献。本文说明如何构建、测试和提交改动。

## 构建

需要 CMake 3.10 或以上版本：

```
cmake -S . -B build
cmake --build build
```

- Linux：gcc / g++ 6.3.0 或以上；
- Windows：Visual Studio 2022（`cmake -S . -B build -A x64`）；
- 交叉编译：使用仓库中的 `3516.toolchain.cmake` 或 `x64.toolchain.cmake`。

## 运行测试

```
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
cd build && ctest --output-on-failure
```

提交前请确保：

- 测试全部通过；
- 三个 `WEBSERVER_TYPE` 变体都能编译：`NONE`、`WEBSOCKET`、`SSE`；
- 若改动了日志模块，本机 Linux 上 `/dev/shm` 可用（单元测试的写文件用例依赖 tmpfs）。

## 代码风格

- 使用仓库根目录的 [.clang-format](.clang-format)（基于 Google 风格，2 空格缩进，120 列）；
- 编辑器配置见 [.editorconfig](.editorconfig)，行尾统一 `LF`，编码 UTF-8；
- 注释与标识符使用英文，避免源文件出现非 ASCII 字符。

## 兼容性要求

以下是硬性要求，评审时会检查：

- **保持 C++11 兼容**，不要使用 `std::filesystem`、`std::string_view`、`std::optional` 等 C++14/17 设施；
- **保持跨平台**，涉及平台差异的代码用 `#ifdef _WIN32` / `#else` 分开；
- **不新增第三方依赖**，除非有充分理由并在 `THIRD_PARTY_NOTICES.md` 中登记。

## 提交信息

采用 Conventional Commits 前缀，冒号后空一格，用英文描述：

```
feat(log): add daily rotation
fix(tcp): handle partial writes
docs: clarify the tmpfs gate
test: cover archive naming collisions
```

## 修改第三方库

`3rdparty/` 下的代码按原样附带，请勿修改。需要升级时，整体替换目录，并同步更新 `THIRD_PARTY_NOTICES.md` 中的版本与许可证信息。

## 报告问题

提交 Issue 时请附上：

- 操作系统与编译器版本；
- 使用的构建选项（尤其是 `WEBSERVER_TYPE`）；
- 复现步骤与相关日志输出。
