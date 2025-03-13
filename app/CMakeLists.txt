cmake_minimum_required(VERSION 3.10)

# 设置项目名称和版本
project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES CXX)
message(STATUS "PROJECT_NAME = " ${PROJECT_NAME})

# 导出Clang需要的文件，用于智能提示和基于语议的补全
SET(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

# spdlog库头文件优化
add_definitions(-DSPDLOG_COMPILED_LIB)

# 设置操作系统相关
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    #set(PTHREAD_NAME "pthreadVC2")
else()
    set(PTHREAD_NAME "pthread")
endif()

# 设置编译器相关
if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_compile_options(/source-charset:utf-8)
    add_compile_options(/execution-charset:utf-8)
endif()

# 设置头文件目录
include_directories(inc)

# 设置链接第三方库的路径
#link_directories(${CMAKE_SOURCE_DIR}/3rdparty/xxx/lib

# 查找所有源文件
aux_source_directory(src SOURCES)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库文件
target_link_libraries(
    ${PROJECT_NAME}
    bl
    spdlog
    unicode_utility
    cmdline
    websocketpp
    asio
    ${PTHREAD_NAME}
)
