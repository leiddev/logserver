cmake_minimum_required(VERSION 3.15)

# The LogServer executable and the applog library it links.
project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES CXX)
message(STATUS "PROJECT_NAME = " ${PROJECT_NAME})

# compile_commands.json for clang based editors and tooling.
SET(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

# Platform specifics.
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
    set(PTHREAD_NAME "pthread")
endif()

# MSVC needs to be told the sources are UTF-8.
if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_compile_options(/source-charset:utf-8)
    add_compile_options(/execution-charset:utf-8)
endif()

include_directories(inc)

# Pick the web front end: NONE, WEBSOCKET or SSE. The value is turned into the
# ENABLE_WEB_SERVER define that main.cc switches on.
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

# The logger is a separate library so the tests can link it on its own.
add_library(applog STATIC src/app_log.cc)
target_include_directories(applog PUBLIC inc)

# Every other source goes into the executable; app_log.cc is already linked.
aux_source_directory(src SOURCES)
list(FILTER SOURCES EXCLUDE REGEX "app_log\\.cc$")

add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_WEB_SERVER=${ENABLE_WEB_SERVER})

target_link_libraries(
    ${PROJECT_NAME}
    applog
    gbcodec
    cmdline
    websocketpp
    asio
    ${PTHREAD_NAME}
)
