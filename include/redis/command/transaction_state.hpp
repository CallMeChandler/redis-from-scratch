#pragma once

#include "redis/protocol/resp_value.hpp"

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

private:
    bool active_ = false;

    std::vector<
        protocol::RespValue
    > queued_commands_;
};

}