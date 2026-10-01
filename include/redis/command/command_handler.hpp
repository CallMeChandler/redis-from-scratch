#pragma once

#include "redis/protocol/resp_value.hpp"

#include <string>
#include <vector>

namespace redis::command {

class CommandHandler {
public:
    [[nodiscard]]
    std::string execute(
        const protocol::RespValue& value
    ) const;

private:
    [[nodiscard]]
    static bool extractArguments(
        const protocol::RespValue& value,
        std::vector<std::string>& arguments
    );

    [[nodiscard]]
    static std::string normalizeCommand(
        std::string command
    );

    [[nodiscard]]
    static std::string executePing(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    static std::string executeEcho(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    static std::string encodeSimpleString(
        const std::string& value
    );

    [[nodiscard]]
    static std::string encodeBulkString(
        const std::string& value
    );

    [[nodiscard]]
    static std::string encodeError(
        const std::string& message
    );
};

}