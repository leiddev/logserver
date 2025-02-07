cmake_minimum_required( VERSION 3.10 FATAL_ERROR)
project(app VERSION 1.0.0 LANGUAGES CXX)
 
#set dirs
message("project dir:${CMAKE_CURRENT_LIST_DIR}")
 
SET(CMAKE_EXPORT_COMPILE_COMMANDS OFF)   #导出clangd需要的文件，用于智能提示和基于语议的补全
 
#SET(BIN_DESTINATION ${PROJECT_SOURCE_DIR}/bin)
#SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BIN_DESTINATION})
#SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BIN_DESTINATION})
#SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DESTINATION})
 
#set compile flags
#add_definitions(-std=c++11 -g -rdynamic)
#add_definitions(-std=c++1y)

#set(CMAKE_CXX_FLAGS "-g3 -rdynamic -std=c++11")
#set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -fsanitize=address -fno-omit-frame-pointer -fsanitize=leak")
#set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

#Strip
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
#set(CMAKE_BUILD_TYPE "RELEASE")

add_definitions(-DSPDLOG_COMPILED_LIB)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	#set(PTHREAD_NAME "pthreadVC2")
	#set(PTHREAD_INC "${PROJECT_ROOT}/3rdparty/pthreadVC2/include")
	#set(PTHREAD_LIB "${PROJECT_ROOT}/3rdparty/pthreadVC2/lib/x86")
else()
	set(PTHREAD_NAME "pthread")
endif()

#include dirs
include_directories(
					./inc 
					${PROJECT_ROOT}/bl/inc
					${PROJECT_ROOT}/3rdparty/spdlog/include
					${PROJECT_ROOT}/3rdparty/tcp_server_client/include
					${PROJECT_ROOT}/3rdparty/cmdline
					${PTHREAD_INC}
					)

#link dirs
link_directories(
#					${PROJECT_ROOT}/3rdparty/xxx/lib
					${PTHREAD_LIB}
					)
#link dirs
#link_directories(${BIN_DESTINATION})
 
#libraries
#SET(SRC_BASE_CALC base_calc.cpp)
#add_library(base_calc SHARED ${SRC_BASE_CALC})
 
#SET(SRC_ADD add_calc.cpp)
#add_library(add SHARED ${SRC_ADD})
#target_link_libraries(add base_calc)
 
#SET(SRC_SUB sub_calc.cpp)
#add_library(sub SHARED ${SRC_SUB})
#arget_link_libraries(sub base_calc)
aux_source_directory(src/ SRC_LIST)

add_executable(LogServer ${SRC_LIST})

target_link_libraries(
	LogServer
	bl
	spdlog
	tcp_server_client
	${PTHREAD_NAME}
)

#execute 
#SET(SRC_MAIN main.cpp calc_service.cpp)
#set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
#add_executable( ${PROJECT_NAME} ${SRC_MAIN})
#target_link_libraries(${PROJECT_NAME} add sub multi div pthread)
