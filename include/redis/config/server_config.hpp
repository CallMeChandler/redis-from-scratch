#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace redis::config {
    
struct ServerConfig {
    std::uint16_t port = 6379;

    std::string replica_of_host;

    std::uint16_t replica_of_port = 0;
    
    [[nodiscard]]
    bool isReplica() const noexcept {
        return !replica_of_host.empty();
    }

    std::string bind_address =
        "0.0.0.0";

    std::string rdb_path =
        "dump.rdb";

    std::string aof_path =
        "appendonly.aof";

    int listen_backlog = 128;

    std::size_t max_clients =
        1024;

    int epoll_timeout_ms =
        100;

    std::size_t expiration_batch_size =
        64;

    std::size_t max_memory_bytes = 0;

    bool save_on_shutdown =
        true;

    [[nodiscard]]
    static ServerConfig loadFromFile(
        const std::string& path
    );
};

} //namespace redis::config