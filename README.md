# 编译方式

使用CMake编译。

## Hi3516 ARM平台

```
mkdir build;cd build

cmake -D CMAKE_TOOLCHAIN_FILE="../3516.toolchain.cmake" ..

make
```

## PC x64平台

```
mkdir build;cd build

cmake -D CMAKE_TOOLCHAIN_FILE="../x64.toolchain.cmake" ..

make
```
