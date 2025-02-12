cmake_minimum_required(VERSION 3.10)

project(LogServer)

set(PROJECT_ROOT ${CMAKE_CURRENT_LIST_DIR})

if(MSVC)
	set(CMAKE_C_FLAGS "${PLATFORM_FLAGS} /execution-charset:gbk")
	set(CMAKE_CXX_FLAGS "${PLATFORM_FLAGS} /EHsc /execution-charset:gbk")
else()
	set(CMAKE_C_FLAGS "-g ${PLATFORM_FLAGS}")
	set(CMAKE_CXX_FLAGS "-g -std=c++11 ${PLATFORM_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath=/mnt/app/lib:/home/root/lib -no-pie")
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)

add_subdirectory(bl ${CMAKE_BINARY_DIR}/out/bl)
add_subdirectory(3rdparty/spdlog ${CMAKE_BINARY_DIR}/out/spdlog)
add_subdirectory(app ${CMAKE_BINARY_DIR}/out/app)

message(STATUS "PLATFORM_FLAGS = " ${PLATFORM_FLAGS})

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

message(STATUS "CMAKE_VERSION = ${CMAKE_VERSION}")

# ---------------------------------------------------------------------------------------
# Check operation system
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
# Check CPU architecture
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
# Check GCC version
# ---------------------------------------------------------------------------------------
if (CMAKE_COMPILER_IS_GNUCC)
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
