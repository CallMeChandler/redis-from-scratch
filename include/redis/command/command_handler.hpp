#pragma once

#include "redis/protocol/resp_value.hpp"
#include "redis/storage/database.hpp"

#include <string>
#include <vector>

namespace redis::command {

class CommandHandler {
public:
    explicit CommandHandler(
        storage::Database& database
    );

    [[nodiscard]]
    std::string execute(
        const protocol::RespValue& value
    );

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
    std::string executeSet(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeGet(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeExists(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeDel(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeHSet(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeHGet(
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
    static std::string encodeNullBulkString();

    [[nodiscard]]
    static std::string encodeInteger(
        long long value
    );

    [[nodiscard]]
    static std::string encodeError(
        const std::string& message
    );

    [[nodiscard]]
    static std::string encodeWrongTypeError();

    storage::Database& database_;
};

}  // namespace redis::command