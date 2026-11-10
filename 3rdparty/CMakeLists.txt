cmake_minimum_required(VERSION 3.15)

# Bundled third party libraries. Each one is vendored unmodified; see
# THIRD_PARTY_NOTICES.md in the repository root for their licenses.

add_subdirectory(cmdline ${CMAKE_BINARY_DIR}/out/3rdparty/cmdline)
add_subdirectory(gbcodec ${CMAKE_BINARY_DIR}/out/3rdparty/gbcodec)
add_subdirectory(websocketpp ${CMAKE_BINARY_DIR}/out/3rdparty/websocketpp)
add_subdirectory(asio ${CMAKE_BINARY_DIR}/out/3rdparty/asio)
