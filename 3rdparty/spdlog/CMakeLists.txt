cmake_minimum_required(VERSION 3.10...3.21)
project(spdlog)

# ---------------------------------------------------------------------------------------
# Compiler config
# ---------------------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# make sure __cplusplus is defined when using msvc and enable parallel build
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /MP")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SYSTEM_NAME MATCHES "CYGWIN" OR CMAKE_SYSTEM_NAME MATCHES "MSYS")
    set(CMAKE_CXX_EXTENSIONS ON)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/EHsc)
endif()

# ---------------------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------------------
find_package(Threads REQUIRED)

add_definitions(-DSPDLOG_COMPILED_LIB)
include_directories(include)
aux_source_directory(src SOURCES)
add_library(${PROJECT_NAME} STATIC ${SOURCES})

# ---------------------------------------------------------------------------------------
# Build example
# --------------------------------------------------------------------------------------
option(EXAMPLE "Build SPDLOG_EXAMPLE" ON)

if(EXAMPLE)
   add_executable(spdlog_example example/example.cpp)
   target_link_libraries(spdlog_example ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
endif()

