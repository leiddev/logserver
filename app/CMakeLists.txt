cmake_minimum_required(VERSION 3.10)

# 设置项目名称和版本
project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES CXX)
message(STATUS "PROJECT_NAME = " ${PROJECT_NAME})

# 导出Clang需要的文件，用于智能提示和基于语议的补全
SET(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

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

# 选择 Web 服务器类型: NONE, WEBSOCKET 或 SSE
set(WEBSERVER_TYPE "SSE" CACHE STRING "Web server type: NONE, WEBSOCKET or SSE")
set_property(CACHE WEBSERVER_TYPE PROPERTY STRINGS NONE WEBSOCKET SSE)

if(WEBSERVER_TYPE STREQUAL "NONE")
    set(ENABLE_WEB_SERVER 0)
elseif(WEBSERVER_TYPE STREQUAL "WEBSOCKET")
    set(ENABLE_WEB_SERVER 1)
elseif(WEBSERVER_TYPE STREQUAL "SSE")
    set(ENABLE_WEB_SERVER 2)
else()
    message(FATAL_ERROR "WEBSERVER_TYPE must be NONE, WEBSOCKET or SSE, got: ${WEBSERVER_TYPE}")
endif()
message(STATUS "WEBSERVER_TYPE = ${WEBSERVER_TYPE} (ENABLE_WEB_SERVER = ${ENABLE_WEB_SERVER})")

# 日志模块独立成库，便于测试直接链接
add_library(applog STATIC src/app_log.cc)
target_include_directories(applog PUBLIC inc)

# 查找所有源文件（app_log.cc 已编入库中）
aux_source_directory(src SOURCES)
list(FILTER SOURCES EXCLUDE REGEX "app_log\\.cc$")

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_WEB_SERVER=${ENABLE_WEB_SERVER})

# 链接库文件
target_link_libraries(
    ${PROJECT_NAME}
    applog
    unicode_utility
    cmdline
    websocketpp
    asio
    ${PTHREAD_NAME}
)
