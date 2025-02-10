# 日志服务器程序

日志服务器程序，用于接收各个应用程序通过UDP发送来的日志，将接收到的日志信息保存成文件，同时通过TCP对外提供日志服务。日志服务器还具备Web前端界面，方面用户通过浏览器查看实时日志信息。

## 开发要求

本程序支持在Linux（x86、ARM）、Windows环境下编译运行，支持Linux x86和ARM间交叉编译。建议在Linux下使用gcc及g++ 6.3.0或以上版本编译，在Windows下使用Microsoft Visual Studio 2022 Community（社区版）编译。建议先在Windows下编译调试好后，再到Linux下编译部署应用。**注意任何对本代码库的修改不得破坏跨平台特性**，在源代码中善用`#ifdef __unix__`和`#ifdef _WIN32`，引入的带三方库也必须具备跨平台特性。

## 主要功能

本程序主要实现了以下功能：

- 可设置接收日志的UDP端口号；
- 可设置对外提供日志的TCP服务端口号；
- 抓取到的日志信息会保持在`/tmp/log/`目录里面（如果该目录为临时文件系统`tmpfs`），自动按照1MB大小分割文件；
- 可设置最大保存文件数量，超过数量则覆盖最旧的文件；
- 可设置是否显示接收到的日志信息；
- 具备Web前端界面，可配置Web服务器端口号；

## 命令行参数

```
usage: ./LogServer [options] ... 
options:
  -u, --udp port     UDP port number (int [=11000])
  -t, --tcp port     TCP port number (int [=11000])
  -s, --max files    Maximum number of files (int [=100])
      --disp_msg     display receive message
  -w, --web port     Web port number (int [=11001])
  -?, --help         print this message
```

## 使用的第三方库

（Linux下均为静态链接使用）

- cmdline：命令行参数解析库；
- spdlog：日志处理库；
- tcp_server_client：TCP服务端与客户端封装库；
- asio：异步IO库；
- WebSocket++：WebSocket协议库（依赖asio）；

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

### Windows平台

使用 Microsoft Visual Studio 2022 Community（社区版）打开包含主CMakeLists.txt的文件夹。

请参阅：  
[Visual Studio 中的 CMake 项目](https://learn.microsoft.com/zh-cn/cpp/build/cmake-projects-in-visual-studio?view=msvc-170)


## 跨平台开发注意事项

**（1）脚本换行格式问题**

使用Visual Studio2022在WSL环境下编译项目，有可能会遇到脚本文件换行符不符合Unxi规范的问题。这是由于Git在Windows下检出代码，会自动将代码换行符改为Windows格式，这样的脚本就无法直接用于Linux系统。解决方法为用vi打开有问题的脚本文件，输入`:set ff`回车，观察是否显示为`fileformat=dos`，如果是，则输入`:set ff=unix`，然后输入`:wq`保存退出。建议将Git设置为保留原本换行符，一劳永逸解决这个问题。具体方法见下。

**（2）Git检出与提交的换行符问题**

在不同的系统平台上，文本文件所使用的换行符是不一样的。Linux和MAC使用的是 `LF`，而 Windows 使用的是 `CRLF`。由于跨平台协作开发非常常见，不统一的换行符确实带来了麻烦。最大的问题是，在不同平台上，换行符发生改变时，Git 会认为整个文件都被修改了，尽管肉眼观察时看不出任何差别。因此，要注意在跨平台协作时统一使用相同的换行符，以避免出现这个问题。建议全部文件使用Linux换行符 `LF`(可在编辑文件时使用编辑器功能手动转换)，并用以下命令进行配置让Git保留原本的换行符：

```
git config --global core.autocrlf false
```

**（3）文件编码问题**

Linux下使用的文件编码为UTF-8，而Windows下使用的文件编码默认为GBK。当源代码文件中没有非ASCII码字符时，Linux下的编译器和Windows下的编译器都能正常工作。但是一旦文件中使用了非ASCII码字符（如中文），由于中文在不同编码下的二进制值不同，某一平台下的编译器将会出现大量错误提示，而令一平台下的编译器却正常。为了避免这种情况，**建议不要再源代码文件中文等非ASCII码字符，注释尽量使用英文。**实在无法避免在源代码中使用中文，可手动将源文件转换为**UTF-8(With BOM)**格式，这样全平台的编译器都能正确识别源文件的编码格式。转换方法可使用编译器自带的编码转换功能，或者使用*超级批量编码转换(UltraCodingSwitch)*软件批量转换。