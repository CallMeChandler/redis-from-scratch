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
    src/persistence/aof.cpp
    src/protocol/resp_parser.cpp
    src/storage/database.cpp
    src/config/server_config.cpp
    src/pubsub/pubsub_manager.cpp
    src/replication/replication_manager.cpp
)

add_executable(
    resp-fuzz
    tests/resp_parser_fuzz.cpp
    src/protocol/resp_parser.cpp
)

foreach(target redis-server resp-fuzz)
    target_include_directories(
        ${target}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )

    target_compile_options(
        ${target}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endforeach()

if (
    CMAKE_CXX_COMPILER_ID MATCHES
    "GNU|Clang"
)
    target_compile_options(
        resp-fuzz
        PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )

    target_link_options(
        resp-fuzz
        PRIVATE
        -fsanitize=address,undefined
    )
endif()