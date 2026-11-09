# SPDX-License-Identifier: MIT

# app_log 单元测试
add_executable(app_log_test app_log_test.cc)
target_link_libraries(app_log_test PRIVATE applog)
add_test(NAME app_log COMMAND app_log_test)

# LogServer 端到端测试（需要 Python 3）
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
