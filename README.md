# 日志服务器程序

日志服务器程序，用于接收各个应用程序通过UDP发送来的日志，将接收到的日志信息保存成文件，同时通过TCP对外提供日志服务。


## 主要功能

本程序主要实现了以下功能：

- 可设置接收日志的UDP端口号；
- 可设置对外提供日志的TCP服务端口号；
- 抓取到的日志信息会保持在`/tmp/log/`目录里面（如果该目录为临时文件系统`tmpfs`），自动按照1MB大小分割文件；
- 可设置最大保存文件数量，超过数量则覆盖最旧的文件；
- 可设置是否显示接收到的日志信息；

## 命令行参数

```
usage: ./LogServer [options] ... 
options:
  -u, --udp port     UDP port number (int [=11000])
  -t, --tcp port     TCP port number (int [=11000])
  -s, --max files    Maximum number of files (int [=100])
      --disp_msg     display receive message
  -?, --help         print this message
```

## 使用的第三方库

（Linux下均为静态链接使用）

- cmdline：命令行参数解析库；
- spdlog：日志处理库；
- tcp_server_client：TCP服务端与客户端封装库；

## 配置文件说明

**（1）主配置文件**

（无）


## 程序编译方式

使用CMake生成编译配置，CMake版本至少应该为3.10。

### Hi3516 ARM平台

```
mkdir build;cd build
cmake -D CMAKE_TOOLCHAIN_FILE="../3516.toolchain.cmake" ..
make
```

### PC Linux平台

```
mkdir build;cd build
cmake -D CMAKE_TOOLCHAIN_FILE="../x64.toolchain.cmake" ..
make
```
