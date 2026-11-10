# SPDX-License-Identifier: MIT
#
# Tests are registered with CTest; no third party test framework is used.
cmake_minimum_required(VERSION 3.15)

# app_log unit tests: formatting, file writes, rotation and recovery.
add_executable(app_log_test app_log_test.cc)
target_link_libraries(app_log_test PRIVATE applog)
add_test(NAME app_log COMMAND app_log_test)

# textenc unit tests: UTF-8 pass-through and the GB2312 fallbacks.
add_executable(text_encoding_test text_encoding_test.cc)
target_link_libraries(text_encoding_test PRIVATE apptext)
add_test(NAME text_encoding COMMAND text_encoding_test)

# End-to-end test of the LogServer executable; needs Python 3.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME server_e2e
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/server_e2e_test.py
                $<TARGET_FILE:LogServer> ${WEBSERVER_TYPE}
    )
    set_tests_properties(server_e2e PROPERTIES TIMEOUT 60)
else()
    message(STATUS "Python3 not found, server_e2e test is skipped")
endif()
