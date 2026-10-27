#pragma once

#include "redis/storage/database.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace redis::replication {

class ReplicationManager {
public:
    void addReplica(
        int client_fd
    );

    void removeReplica(
        int client_fd
    );

    [[nodiscard]]
    const std::unordered_set<int>& replicas() const noexcept;

    [[nodiscard]]
    static bool isWriteCommand(
        const std::string& command
    );

    [[nodiscard]]
    static std::string encodeCommand(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    static std::string buildFullSync(
        storage::Database& database
    );

private:
    std::unordered_set<int> replicas_;
};

}