cmake_minimum_required(VERSION 3.15...3.31)

project(gbcodec
    VERSION 1.2.1
    DESCRIPTION "GB2312 <-> Unicode (UTF-8/UTF-16) conversion library"
    HOMEPAGE_URL "https://github.com/leiddev/gbcodec"
    LANGUAGES C)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(GBCODEC_IS_TOP_LEVEL ON)
else()
    set(GBCODEC_IS_TOP_LEVEL OFF)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(GBCODEC_BUILD_CXX "Provide the C++ convenience wrapper" ${GBCODEC_IS_TOP_LEVEL})
option(GBCODEC_BUILD_EXAMPLES "Build the example programs" ${GBCODEC_IS_TOP_LEVEL})
option(GBCODEC_BUILD_TESTS "Build the unit tests" ${GBCODEC_IS_TOP_LEVEL})
option(GBCODEC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(GBCODEC_BUILD_CXX)
    enable_language(CXX)
endif()

# Single-configuration generators need an explicit default build type.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# Programs and DLLs share one output directory per configuration so that a
# shared-library build can be run without copying files around.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Warning helper
# ---------------------------------------------------------------------------
function(gbcodec_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(GBCODEC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
        if(GBCODEC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Generated headers
# ---------------------------------------------------------------------------
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gbcodec/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/gbcodec/version.h
    @ONLY)

# ---------------------------------------------------------------------------
# Library: C core
# ---------------------------------------------------------------------------
add_library(gbcodec
    src/convert.c
    src/gb2312.c
    src/status.c
    src/utf8.c
    src/utf16.c
    src/detail/gb2312_table.c)
add_library(gbcodec::gbcodec ALIAS gbcodec)

target_include_directories(gbcodec PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_include_directories(gbcodec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Only the symbols marked GBC_API leave a shared library.
target_compile_definitions(gbcodec PRIVATE GBCODEC_BUILDING_LIBRARY)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(gbcodec PUBLIC GBCODEC_SHARED)
endif()

set_target_properties(gbcodec PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

gbcodec_enable_warnings(gbcodec)

# ---------------------------------------------------------------------------
# Library: C++ convenience wrapper (header-only, C++11 or later)
# ---------------------------------------------------------------------------
if(GBCODEC_BUILD_CXX)
    add_library(gbcodec_cpp INTERFACE)
    add_library(gbcodec::gbcodec_cpp ALIAS gbcodec_cpp)

    target_link_libraries(gbcodec_cpp INTERFACE gbcodec)
    target_compile_features(gbcodec_cpp INTERFACE cxx_std_11)

    if(MSVC)
        target_compile_options(gbcodec_cpp INTERFACE /utf-8)
    endif()
endif()

# ---------------------------------------------------------------------------
# Installation and package configuration
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS gbcodec
    EXPORT gbcodecTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(GBCODEC_BUILD_CXX)
    install(TARGETS gbcodec_cpp EXPORT gbcodecTargets)
endif()

install(DIRECTORY include/gbcodec
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp")

# version.h is generated into the build tree.
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/gbcodec/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gbcodec)

install(EXPORT gbcodecTargets
    NAMESPACE gbcodec::
    FILE gbcodec-targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gbcodec)

configure_package_config_file(
    cmake/gbcodec-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/gbcodec-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gbcodec)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/gbcodec-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/gbcodec-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/gbcodec-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gbcodec)

configure_file(cmake/gbcodec.pc.in ${CMAKE_CURRENT_BINARY_DIR}/gbcodec.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gbcodec.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# ---------------------------------------------------------------------------
# Tests and examples
# ---------------------------------------------------------------------------
if(GBCODEC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GBCODEC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
