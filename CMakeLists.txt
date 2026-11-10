cmake_minimum_required(VERSION 3.10)

# LogServer: UDP log collector with TCP, SSE and WebSocket front ends.
# This file drives the build for every supported platform; the toolchain
# files select the compiler and the target flags.

message(STATUS "CMAKE_VERSION = ${CMAKE_VERSION}")

project(LogServer VERSION 1.0.0)

# Echo every compiler and linker invocation.
set(CMAKE_VERBOSE_MAKEFILE ON)

# Platform flags come from the toolchain file, when one is used.
message(STATUS "PLATFORM_FLAGS = " ${PLATFORM_FLAGS})

# Language level and warning suppressions; C++11 is the project baseline.
if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_FLAGS} -g -std=c11")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_FLAGS} -g -std=c++11")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ")
elseif(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_FLAGS} /std:c11 /wd5105")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_FLAGS} /EHsc /std:c++14")
endif()

# Bundled third party libraries.
add_subdirectory(3rdparty ${CMAKE_BINARY_DIR}/out/3rdparty)

# The server itself.
add_subdirectory(app ${CMAKE_BINARY_DIR}/out/app)

# Tests are built by default; pass -DBUILD_TESTING=OFF to skip them.
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests ${CMAKE_BINARY_DIR}/out/tests)
endif()

# ---------------------------------------------------------------------------------------
# Dump the compiler and linker flags for the chosen configuration.
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
# Host and target operating system.
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
# Host and target CPU architecture.
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
# Compiler identification and version.
# ---------------------------------------------------------------------------------------
message(STATUS "C++ compiler id: " ${CMAKE_CXX_COMPILER_ID})
message(STATUS "C++ compiler version is: " ${CMAKE_CXX_COMPILER_VERSION})

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpfullversion -dumpversion
        OUTPUT_VARIABLE GXX_VERSION)
    string(REGEX MATCHALL "[0-9]+" GXX_VERSION_COMPONENTS ${GXX_VERSION})
    list(GET GXX_VERSION_COMPONENTS 0 GXX_MAJOR)
    list(GET GXX_VERSION_COMPONENTS 1 GXX_MINOR)

    set(GXX_VERSION "${GXX_MAJOR}.${GXX_MINOR}")
    message(STATUS "g++ version: ${GXX_VERSION}")
endif()

message(STATUS "C compiler id: ${CMAKE_C_COMPILER_ID}")
message(STATUS "C compiler version is: ${CMAKE_C_COMPILER_VERSION}")

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpfullversion -dumpversion
        OUTPUT_VARIABLE GCC_VERSION)
    string(REGEX MATCHALL "[0-9]+" GCC_VERSION_COMPONENTS ${GCC_VERSION})
    list(GET GCC_VERSION_COMPONENTS 0 GCC_MAJOR)
    list(GET GCC_VERSION_COMPONENTS 1 GCC_MINOR)

    set(GCC_VERSION "${GCC_MAJOR}.${GCC_MINOR}")
    message(STATUS "gcc version: ${GCC_VERSION}")
endif()