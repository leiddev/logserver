cmake_minimum_required(VERSION 3.10)

# 显示CMake版本
message(STATUS "CMAKE_VERSION = ${CMAKE_VERSION}")

# 设置项目名称和版本
project(LogServer VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 启用详细输出
set(CMAKE_VERBOSE_MAKEFILE ON)

# 包含第三方库目录
add_subdirectory(3rdparty ${CMAKE_BINARY_DIR}/out/3rdparty)

# 添加 app 子目录
add_subdirectory(app ${CMAKE_BINARY_DIR}/out/app)

# 显示编译平台相关设置
message(STATUS "PLATFORM_FLAGS = " ${PLATFORM_FLAGS})

# 编译器相关选项
add_compile_options(${PLATFORM_FLAGS})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-g)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/EHsc)
endif()

# ---------------------------------------------------------------------------------------
# 显示编译器和链接器设置
# ---------------------------------------------------------------------------------------
message(STATUS "CMAKE_C_FLAGS = " ${CMAKE_C_FLAGS})
message(STATUS "CMAKE_C_FLAGS_DEBUG = " ${CMAKE_C_FLAGS_DEBUG})
message(STATUS "CMAKE_C_FLAGS_MINSIZEREL = " ${CMAKE_C_FLAGS_MINSIZEREL})
message(STATUS "CMAKE_C_FLAGS_RELEASE = " ${CMAKE_C_FLAGS_RELEASE})
message(STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO = " ${CMAKE_C_FLAGS_RELWITHDEBINFO})

message(STATUS "CMAKE_CXX_FLAGS = " ${CMAKE_CXX_FLAGS})
message(STATUS "CMAKE_CXX_FLAGS_DEBUG = " ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "CMAKE_CXX_FLAGS_MINSIZEREL = " ${CMAKE_CXX_FLAGS_MINSIZEREL})
message(STATUS "CMAKE_CXX_FLAGS_RELEASE = " ${CMAKE_CXX_FLAGS_RELEASE})
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO = " ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})

message(STATUS "CMAKE_EXE_LINKER_FLAGS = " ${CMAKE_EXE_LINKER_FLAGS})
message(STATUS "CMAKE_EXE_LINKER_FLAGS_DEBUG = " ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
message(STATUS "CMAKE_EXE_LINKER_FLAGS_MINSIZEREL = " ${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL})
message(STATUS "CMAKE_EXE_LINKER_FLAGS_RELEASE = " ${CMAKE_EXE_LINKER_FLAGS_RELEASE})
message(STATUS "CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO = " ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})

message(STATUS "CMAKE_MODULE_LINKER_FLAGS = " ${CMAKE_MODULE_LINKER_FLAGS})
message(STATUS "CMAKE_MODULE_LINKER_FLAGS_DEBUG = " ${CMAKE_MODULE_LINKER_FLAGS_DEBUG})
message(STATUS "CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL = " ${CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL})
message(STATUS "CMAKE_MODULE_LINKER_FLAGS_RELEASE = " ${CMAKE_MODULE_LINKER_FLAGS_RELEASE})
message(STATUS "CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO = " ${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO})

message(STATUS "CMAKE_SHARED_LINKER_FLAGS = " ${CMAKE_SHARED_LINKER_FLAGS})
message(STATUS "CMAKE_SHARED_LINKER_FLAGS_DEBUG = " ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
message(STATUS "CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL = " ${CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL})
message(STATUS "CMAKE_SHARED_LINKER_FLAGS_RELEASE = " ${CMAKE_SHARED_LINKER_FLAGS_RELEASE})
message(STATUS "CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO = " ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})

# ---------------------------------------------------------------------------------------
# 显示操作系统信息
# ---------------------------------------------------------------------------------------
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
	message(STATUS "Current OS is Linux.")
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
	message(STATUS "Current OS is Windows.")
else()
	message(STATUS "Current OS is ${CMAKE_HOST_SYSTEM_NAME}.")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	message(STATUS "Target OS is Linux.")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
	message(STATUS "Target OS is Windows.")
else()
	message(STATUS "Target OS is ${CMAKE_SYSTEM_NAME}.")
endif()

# ---------------------------------------------------------------------------------------
# 显示CPU架构信息
# ---------------------------------------------------------------------------------------
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
	message(STATUS "Current CPU is x86_64 CPU.")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
	message(STATUS "Current CPU is aarch64 CPU.")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv7l")
	message(STATUS "Current CPU is armv7l CPU.")
else()
	message(STATUS "Current CPU is ${CMAKE_HOST_SYSTEM_PROCESSOR}.")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	message(STATUS "Target CPU is x86_64 CPU.")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
	message(STATUS "Target CPU is aarch64 CPU.")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
	message(STATUS "Target CPU is armv7l CPU.")
else()
	message(STATUS "Target CPU is ${CMAKE_SYSTEM_PROCESSOR}.")
endif()

# ---------------------------------------------------------------------------------------
# 显示编译器信息
# ---------------------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpfullversion -dumpversion
        OUTPUT_VARIABLE GCC_VERSION)
    string(REGEX MATCHALL "[0-9]+" GCC_VERSION_COMPONENTS ${GCC_VERSION})
    list(GET GCC_VERSION_COMPONENTS 0 GCC_MAJOR)
    list(GET GCC_VERSION_COMPONENTS 1 GCC_MINOR)

    set(GCC_VERSION "${GCC_MAJOR}.${GCC_MINOR}")
    message(STATUS "gcc version: ${GCC_VERSION}")

    if (GCC_VERSION GREATER "6.2")
        message(STATUS "gcc version major: ${GCC_MAJOR}")
        message(STATUS "gcc version minor: ${GCC_MINOR}")
    endif()
else()
	message(STATUS "C compiler id: ${CMAKE_C_COMPILER_ID}")
	message(STATUS "C compiler version is: ${CMAKE_C_COMPILER_VERSION}")
endif()
