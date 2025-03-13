cmake_minimum_required(VERSION 3.10)
project(websocketpp)

# ---------------------------------------------------------------------------------------
# Compiler config
# ---------------------------------------------------------------------------------------



# ---------------------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------------------

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
