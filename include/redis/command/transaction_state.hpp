#pragma once

#include "redis/protocol/resp_value.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace redis::command {

class TransactionState {
public:
    [[nodiscard]]
    bool active() const noexcept {
        return active_;
    }

    void begin() {
        active_ = true;
        queued_commands_.clear();
    }

    void discard() {
        active_ = false;
        queued_commands_.clear();
        watched_versions_.clear();
    }

    void queue(
        protocol::RespValue command
    ) {
        queued_commands_.push_back(
            std::move(command)
        );
    }

    [[nodiscard]]
    std::vector<protocol::RespValue> takeQueueCommands() {
        active_ = false;

        std::vector<protocol::RespValue> commands =
            std::move(queued_commands_);

        queued_commands_.clear();

        return commands;
    }

    void watch(
        const std::string& key,
        std::uint64_t version
    ) {
        watched_versions_.insert_or_assign(
            std::move(key),
            version
        );
    }

    void unwatch(){
        watched_versions_.clear();
    }

    [[nodiscard]]
    const std::unordered_map<
        std::string,
        std::uint64_t
    >& watchedVersions() const noexcept {
        return watched_versions_;
    }

private:
    bool active_ = false;

    std::vector<
        protocol::RespValue
    > queued_commands_;

    std::unordered_map<
        std::string,
        std::uint64_t
    > watched_versions_;
};

}