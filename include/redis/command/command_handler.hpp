#pragma once

#include "redis/command/transaction_state.hpp"
#include "redis/persistence/aof.hpp"
#include "redis/persistence/rdb.hpp"
#include "redis/protocol/resp_value.hpp"
#include "redis/storage/database.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace redis::command {

class CommandHandler {
public:
    CommandHandler(
        storage::Database& database,
        persistence::RdbSnapshot& snapshot,
        persistence::AppendOnlyFile& aof
    );

    [[nodiscard]]
    std::string execute(
        const protocol::RespValue& value,
        TransactionState& transaction
    );

    [[nodiscard]]
    std::string replay(
        const protocol::RespValue& value
    );

    [[nodiscard]]
    static bool decodeCommand(
        const protocol::RespValue& value,
        std::vector<std::string>& arguments,
        std::string& command
    );

    using WriteObserver =
        std::function<
            void(
                const std::vector<std::string>&
            )
        >;

    void setWriteObserver(
        WriteObserver observer
    );

private:
    using CommandFunction =
        std::string (
            CommandHandler::*
        )(
            const std::vector<std::string>&
        );

    struct CommandDefinition {
        CommandFunction handler;
        bool writes_data;
        bool touches_key = false;
    };

    using CommandRegistry =
        std::unordered_map<
            std::string,
            CommandDefinition
        >;

    [[nodiscard]]
    static CommandRegistry createCommandRegistry();

    [[nodiscard]]
    std::string executeImmediate(
        const protocol::RespValue& value,
        bool append_to_aof
    );

    [[nodiscard]]
    std::string executeTransaction(
        const protocol::RespValue& value,
        const std::vector<std::string>& arguments,
        TransactionState& transaction
    );

    [[nodiscard]]
    std::string executeExec(
        TransactionState& transaction
    );

    [[nodiscard]]
    std::string executeWatch(
        const std::vector<std::string>& arguments,
        TransactionState& transaction
    );

    [[nodiscard]]
    std::string executeUnwatch(
        const std::vector<std::string>& arguments,
        TransactionState& transaction
    );

    [[nodiscard]]
    bool watchedKeysChanged(
        const TransactionState& transaction
    );

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
    static bool parseInteger(
        const std::string& text,
        long long& value
    );

    [[nodiscard]]
    static bool parseDouble(
        const std::string& text,
        double& value
    );

    [[nodiscard]]
    std::string executePing(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeEcho(
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
    std::string executeLPush(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeRPush(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeLPop(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeRPop(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeLRange(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeLLen(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeSAdd(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeSRem(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeSIsMember(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeSMembers(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeZAdd(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeZScore(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeZRange(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeExpire(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeTTL(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeSave(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeRewriteAof(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    std::string executeMemoryUsage(
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
    static std::string encodeArray(
        const std::vector<std::string>& values
    );

    [[nodiscard]]
    static std::string encodeRawArray(
        const std::vector<std::string>& responses
    );

    [[nodiscard]]
    static std::string encodeNullArray();

    [[nodiscard]]
    static std::string encodeError(
        const std::string& message
    );

    [[nodiscard]]
    static std::string encodeWrongTypeError();

    storage::Database& database_;

    persistence::RdbSnapshot& snapshot_;

    persistence::AppendOnlyFile& aof_;

    CommandRegistry command_registry_;

    WriteObserver write_observer_;
};

}  // namespace redis::command