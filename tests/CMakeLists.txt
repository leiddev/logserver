# SPDX-License-Identifier: MIT
#
# Tests are registered with CTest; no third party test framework is used.
cmake_minimum_required(VERSION 3.15)

# Two tests compile app/src/html_page.cc, which embeds UTF-8 viewer pages;
# without this MSVC warns C4819 on every page included into those targets.
if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_compile_options(/source-charset:utf-8)
    add_compile_options(/execution-charset:utf-8)
endif()

# app_log unit tests: formatting, file writes, rotation and recovery.
add_executable(app_log_test app_log_test.cc)
target_link_libraries(app_log_test PRIVATE applog)
add_test(NAME app_log COMMAND app_log_test)

# textenc unit tests: UTF-8 pass-through and the GB2312 fallbacks.
add_executable(text_encoding_test text_encoding_test.cc)
target_link_libraries(text_encoding_test PRIVATE apptext)
add_test(NAME text_encoding COMMAND text_encoding_test)

find_package(Threads REQUIRED)

# These tests compile html_page.cc, which #includes the page files; the
# compiler's dependency scan does not report them, so name them here.
set(PAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app/inc/html")
set_source_files_properties(../app/src/html_page.cc PROPERTIES OBJECT_DEPENDS
    "${PAGE_DIR}/log_page_head.html;${PAGE_DIR}/log_page_sse.html;${PAGE_DIR}/log_page_ws.html;${PAGE_DIR}/log_page_tail.html")

# Shutdown guard: stop() must not leave process_messages() waiting when the
# endpoint never started listening.
add_executable(websocket_stop_test websocket_stop_test.cc ../app/src/html_page.cc)
target_link_libraries(websocket_stop_test PRIVATE applog apptext websocketpp asio Threads::Threads)
add_test(NAME websocket_stop COMMAND websocket_stop_test)
set_tests_properties(websocket_stop PROPERTIES TIMEOUT 60)

# Guard against the m_connections data race coming back. Only ThreadSanitizer
# can see it, so the binary is built with the flag explicitly and kept behind
# an option: a sanitizer runtime does not belong in a normal release build.
option(LOG_SERVER_TSAN_TESTS "Build the ThreadSanitizer WebSocket race test" OFF)
if(LOG_SERVER_TSAN_TESTS)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(websocket_broadcast_race_test
            websocket_broadcast_race_test.cc
            ../app/src/html_page.cc)
        target_compile_options(websocket_broadcast_race_test PRIVATE
            -fsanitize=thread -fno-omit-frame-pointer -g -O1)
        target_link_options(websocket_broadcast_race_test PRIVATE -fsanitize=thread)
        target_link_libraries(websocket_broadcast_race_test PRIVATE
            applog apptext websocketpp asio Threads::Threads)
        add_test(NAME websocket_broadcast_race COMMAND websocket_broadcast_race_test)
        set_tests_properties(websocket_broadcast_race PROPERTIES TIMEOUT 120)
    else()
        message(WARNING "LOG_SERVER_TSAN_TESTS needs Linux with GCC or Clang; the race reproducer is skipped")
    endif()
endif()

# Browser-level tests of the log viewer page, driven through Playwright. It
# needs Chromium, a large download, so it stays off unless asked for; CI
# installs Playwright and turns it on for the SSE and WebSocket builds.
option(LOG_SERVER_UI_TESTS "Run the browser UI tests with Playwright" OFF)

# End-to-end test of the LogServer executable; needs Python 3.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME server_e2e
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/server_e2e_test.py
                $<TARGET_FILE:LogServer> ${WEBSERVER_TYPE}
    )
    set_tests_properties(server_e2e PROPERTIES TIMEOUT 60)

    # Stability, robustness and availability checks for the web front ends:
    # slow consumers, event integrity, disconnects and client churn.
    if(NOT WEBSERVER_TYPE STREQUAL "NONE")
        add_test(
            NAME web_frontend
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/web_frontend_test.py
                    $<TARGET_FILE:LogServer> ${WEBSERVER_TYPE}
        )
        set_tests_properties(web_frontend PROPERTIES TIMEOUT 180)

        # The page itself in a real Chromium: rendering, trimming, clearing,
        # saving, auto-scroll and reconnection.
        if(LOG_SERVER_UI_TESTS)
            add_test(
                NAME web_ui
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/web_ui_test.py
                        $<TARGET_FILE:LogServer> ${WEBSERVER_TYPE}
            )
            set_tests_properties(web_ui PROPERTIES TIMEOUT 300)
        endif()
    endif()

    # The UDP producer and the TCP consumer side: burst loss and ordering,
    # maximum and empty datagrams, fan-out under load, slow consumers, churn
    # and the shutdown path. Runs for every web server type.
    add_test(
        NAME transport_stability
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/transport_stability_test.py
                $<TARGET_FILE:LogServer> ${WEBSERVER_TYPE}
    )
    set_tests_properties(transport_stability PROPERTIES TIMEOUT 180)
else()
    message(STATUS "Python3 not found, server_e2e test is skipped")
endif()
