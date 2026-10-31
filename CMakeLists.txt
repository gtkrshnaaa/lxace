cmake_minimum_required(VERSION 3.20)
project(lxace VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings and threading
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -pthread
)

set(CORE_SOURCES
    src/core/config.cpp
    src/core/event_bus.cpp
    src/core/engine.cpp
    src/sys/fs.cpp
    src/sys/watcher.cpp
    src/sys/process.cpp
    src/sys/procfs.cpp
    src/sys/network.cpp
    src/json/json_value.cpp
    src/json/json_parser.cpp
    src/json/json_writer.cpp
    src/net/socket.cpp
    src/net/epoll_reactor.cpp
    src/net/http_parser.cpp
    src/net/http_router.cpp
    src/net/sse_stream.cpp
    src/net/http_server.cpp
    src/agent/sub_agent.cpp
    src/agent/orchestrator.cpp
    src/cli/terminal.cpp
    src/cli/view_home.cpp
    src/cli/view_settings.cpp
    src/cli/view_prompt.cpp
    src/cli/cli_app.cpp
)

# Main executable
add_executable(lxace
    src/main.cpp
    ${CORE_SOURCES}
)

target_include_directories(lxace PRIVATE src)
target_link_libraries(lxace PRIVATE pthread)

# Test executable
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_ring_buffer.cpp
    tests/test_json.cpp
    tests/test_fs.cpp
    tests/test_process.cpp
    tests/test_http.cpp
)

add_executable(lxace_tests
    ${TEST_SOURCES}
    ${CORE_SOURCES}
)

target_include_directories(lxace_tests PRIVATE src tests)
target_link_libraries(lxace_tests PRIVATE pthread)
