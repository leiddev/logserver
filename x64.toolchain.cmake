# this is required
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# specify the cross compiler
set(CMAKE_C_COMPILER   gcc)
set(CMAKE_CXX_COMPILER g++)

# search for programs in the build host directories (not necessary)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(PLATFORM_FLAGS "-fno-aggressive-loop-optimizations -ldl -ffunction-sections -fdata-sections -fstack-protector-strong -fPIC")
#set(CMAKE_C_FLAGS "${PLATFORM_FLAGS}")
#set(CMAKE_CXX_FLAGS "-std=c++1y ${PLATFORM_FLAGS}")
