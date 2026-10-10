cmake_minimum_required(VERSION 3.20)

project(
    redis_from_scratch
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    redis-server
    src/main.cpp
    src/command/command_handler.cpp
    src/net/connection.cpp
    src/net/tcp_server.cpp
    src/persistence/rdb.cpp
    src/protocol/resp_parser.cpp
    src/storage/database.cpp
)

target_include_directories(
    redis-server
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(
    redis-server
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)