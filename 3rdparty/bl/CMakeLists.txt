cmake_minimum_required(VERSION 3.10)

project(bl VERSION 1.0.0 LANGUAGES CXX)

add_definitions(-DSPDLOG_COMPILED_LIB)
if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

aux_source_directory(src SOURCES)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC inc)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/3rdparty/spdlog/include)
