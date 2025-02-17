cmake_minimum_required(VERSION 3.10)
project(unicode_utility)

# ---------------------------------------------------------------------------------------
# Compiler config
# ---------------------------------------------------------------------------------------



# ---------------------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------------------

include_directories(inc)
aux_source_directory(src/ SRC_LIST)
add_library(${PROJECT_NAME} STATIC ${SRC_LIST})

option(EXAMPLE "Build EXAMPLE" ON)

if(EXAMPLE)

    add_executable(unicode_example examples/unicode_example.c)
    target_link_libraries (unicode_example ${PROJECT_NAME})

    add_executable(unicode_cpp_example examples/unicode_cpp_example.cpp)
    target_link_libraries (unicode_cpp_example ${PROJECT_NAME})

endif()
