#include "redis/command/command_handler.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string>
#include <variant>
#include <vector>

namespace redis::command {

CommandHandler::CommandHandler(
    storage::Database& database,
    persistence::RdbSnapshot& snapshot
)
    : database_(database),
      snapshot_(snapshot) {
}

std::string CommandHandler::execute(
    const protocol::RespValue& value,
    TransactionState& transaction
) {
    std::vector<std::string> arguments;

    if (
        !extractArguments(
            value,
            arguments
        )
    ) {
        return encodeError(
            "ERR command must be an array of bulk strings"
        );
    }

    if (arguments.empty()) {
        return encodeError(
            "ERR empty command"
        );
    }

    return executeTransaction(
        value,
        arguments,
        transaction
    );
}

std::string CommandHandler::executeTransaction(
    const protocol::RespValue& value,
    const std::vector<std::string>& arguments,
    TransactionState& transaction
) {
    const std::string command =
        normalizeCommand(
            arguments[0]
        );

    if (command == "WATCH"){
        return executeWatch(
            arguments,
            transaction
        );
    }

    if (command == "UNWATCH") {
        return executeUnwatch(
            arguments,
            transaction
        );
    }

    if (command == "MULTI") {
        if (arguments.size() != 1) {
            return encodeError(
                "ERR wrong number of arguments for 'multi' command"
            );
        }

        if (transaction.active()) {
            return encodeError(
                "ERR MULTI calls can not be nested"
            );
        }

        transaction.begin();

        return encodeSimpleString(
            "OK"
        );
    }

    if (command == "DISCARD") {
        if (arguments.size() != 1) {
            return encodeError(
                "ERR wrong number of arguments for 'discard' command"
            );
        }

        if (!transaction.active()) {
            return encodeError(
                "ERR DISCARD without MULTI"
            );
        }

        transaction.discard();

        return encodeSimpleString(
            "OK"
        );
    }

    if (command == "EXEC") {
        if (arguments.size() != 1) {
            return encodeError(
                "ERR wrong number of arguments for 'exec' command"
            );
        }

        if (!transaction.active()) {
            return encodeError(
                "ERR EXEC without MULTI"
            );
        }

        return executeExec(
            transaction
        );
    }

    if (transaction.active()) {
        transaction.queue(
            value
        );

        return encodeSimpleString(
            "QUEUED"
        );
    }

    return executeImmediate(
        value
    );
}

std::string CommandHandler::executeExec(
    TransactionState& transaction
) {
    if (
        watchedKeysChanged(
            transaction
        )
    ) {
        transaction.discard();

        return encodeNullArray();
    }

    std::vector<protocol::RespValue> commands =
        transaction.takeQueueCommands();

    transaction.unwatch();

    std::vector<std::string> responses;

    responses.reserve(
        commands.size()
    );

    for (
        const protocol::RespValue& command :
        commands
    ) {
        responses.push_back(
            executeImmediate(command)
        );
    }

    return encodeRawArray(
        responses
    );
}

std::string CommandHandler::executeWatch(
    const std::vector<std::string>& arguments,
    TransactionState& transaction
) {
    if (arguments.size() < 2){
        return encodeError(
            "ERR Wrong number of arguments for 'watch' command"
        );
    }

    if (transaction.active()) {
        return encodeError(
            "ERR WATCH inside MULTI is not allowed"
        );
    }

    for (
        std::size_t index = 1;
        index < arguments.size();
        ++index
    ) {
        const std::string& key =
            arguments[index];

        transaction.watch(
            key,
            database_.keyVersion(key)
        );
    }

    return encodeSimpleString(
        "OK"
    );
}

std::string CommandHandler::executeUnwatch(
    const std::vector<std::string>& arguments,
    TransactionState& transaction
) {
    if (arguments.size()!=1){
        return encodeError(
            "ERR wrong number of arguments for 'unwatch' command"
        );
    }

    transaction.unwatch();

    return encodeSimpleString(
        "OK"
    );
}

bool CommandHandler::watchedKeysChanged(
    const TransactionState& transaction
) {
    for (
        const auto& entry :
        transaction.watchedVersions()
    ) {
        const std::string& key =
            entry.first;

        const std::uint64_t expected_version =
            entry.second;

        const std::uint64_t current_version =
            database_.keyVersion(key);

        if (
            current_version !=
            expected_version
        ) {
            return true;
        }
    }

    return false;
}

std::string CommandHandler::executeImmediate(
    const protocol::RespValue& value
) {
    std::vector<std::string> arguments;

    if (!extractArguments(value, arguments)) {
        return encodeError(
            "ERR command must be an array of bulk strings"
        );
    }

    if (arguments.empty()) {
        return encodeError(
            "ERR empty command"
        );
    }

    const std::string command =
        normalizeCommand(arguments[0]);

    if (command == "PING") {
        return executePing(arguments);
    }

    if (command == "ECHO") {
        return executeEcho(arguments);
    }

    if (command == "SET") {
        return executeSet(arguments);
    }

    if (command == "GET") {
        return executeGet(arguments);
    }

    if (command == "EXISTS") {
        return executeExists(arguments);
    }

    if (command == "DEL") {
        return executeDel(arguments);
    }

    if (command == "HSET") {
        return executeHSet(arguments);
    }

    if (command == "HGET") {
        return executeHGet(arguments);
    }

    if (command == "LPUSH") {
        return executeLPush(arguments);
    }

    if (command == "RPUSH") {
        return executeRPush(arguments);
    }

    if (command == "LPOP") {
        return executeLPop(arguments);
    }

    if (command == "RPOP") {
        return executeRPop(arguments);
    }

    if (command == "LRANGE") {
        return executeLRange(arguments);
    }

    if (command == "LLEN") {
        return executeLLen(arguments);
    }

    if (command == "SADD") {
        return executeSAdd(arguments);
    }

    if (command == "SREM") {
        return executeSRem(arguments);
    }

    if (command == "SISMEMBER") {
        return executeSIsMember(arguments);
    }

    if (command == "SMEMBERS") {
        return executeSMembers(arguments);
    }

    if (command == "ZADD") {
        return executeZAdd(arguments);
    }

    if (command == "ZSCORE") {
        return executeZScore(arguments);
    }

    if (command == "ZRANGE") {
        return executeZRange(arguments);
    }

    if (command == "EXPIRE") {
        return executeExpire(arguments);
    }

    if (command == "TTL") {
        return executeTTL(arguments);
    }

    if (command == "SAVE") {
        return executeSave(arguments);
    }

    return encodeError(
        "ERR unknown command '" +
        arguments[0] +
        "'"
    );
}

bool CommandHandler::extractArguments(
    const protocol::RespValue& value,
    std::vector<std::string>& arguments
) {
    const protocol::RespArray* array =
        std::get_if<protocol::RespArray>(
            &value.value
        );

    if (array == nullptr) {
        return false;
    }

    arguments.clear();
    arguments.reserve(array->size());

    for (
        const protocol::RespValue& element :
        *array
    ) {
        const protocol::BulkString* bulk =
            std::get_if<protocol::BulkString>(
                &element.value
            );

        if (bulk != nullptr) {
            arguments.push_back(
                bulk->value
            );
            continue;
        }

        const protocol::SimpleString* simple =
            std::get_if<protocol::SimpleString>(
                &element.value
            );

        if (simple != nullptr) {
            arguments.push_back(
                simple->value
            );
            continue;
        }

        return false;
    }

    return true;
}

std::string CommandHandler::normalizeCommand(
    std::string command
) {
    std::transform(
        command.begin(),
        command.end(),
        command.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::toupper(character)
            );
        }
    );

    return command;
}

bool CommandHandler::parseInteger(
    const std::string& text,
    long long& value
) {
    const char* begin = text.data();
    const char* end =
        text.data() + text.size();

    const auto result =
        std::from_chars(
            begin,
            end,
            value
        );

    return result.ec == std::errc{} &&
           result.ptr == end;
}

bool CommandHandler::parseDouble(
    const std::string& text,
    double& value
) {
    char* end_pointer = nullptr;

    value =
        std::strtod(
            text.c_str(),
            &end_pointer
        );

    return end_pointer !=
               text.c_str() &&
           *end_pointer == '\0';
}

std::string CommandHandler::executePing(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() == 1) {
        return encodeSimpleString("PONG");
    }

    if (arguments.size() == 2) {
        return encodeBulkString(
            arguments[1]
        );
    }

    return encodeError(
        "ERR wrong number of arguments for 'ping' command"
    );
}

std::string CommandHandler::executeEcho(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 2) {
        return encodeError(
            "ERR wrong number of arguments for 'echo' command"
        );
    }

    return encodeBulkString(
        arguments[1]
    );
}

std::string CommandHandler::executeSet(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'set' command"
        );
    }

    database_.setString(
        arguments[1],
        arguments[2]
    );

    return encodeSimpleString("OK");
}

std::string CommandHandler::executeGet(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 2) {
        return encodeError(
            "ERR wrong number of arguments for 'get' command"
        );
    }

    const storage::StringLookupResult result =
        database_.getString(
            arguments[1]
        );

    if (
        result.status ==
        storage::LookupStatus::Missing
    ) {
        return encodeNullBulkString();
    }

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeBulkString(
        result.value
    );
}

std::string CommandHandler::executeExists(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() < 2) {
        return encodeError(
            "ERR wrong number of arguments for 'exists' command"
        );
    }

    long long count = 0;

    for (
        std::size_t index = 1;
        index < arguments.size();
        ++index
    ) {
        if (
            database_.exists(
                arguments[index]
            )
        ) {
            ++count;
        }
    }

    return encodeInteger(count);
}

std::string CommandHandler::executeDel(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() < 2) {
        return encodeError(
            "ERR wrong number of arguments for 'del' command"
        );
    }

    long long count = 0;

    for (
        std::size_t index = 1;
        index < arguments.size();
        ++index
    ) {
        if (
            database_.erase(
                arguments[index]
            )
        ) {
            ++count;
        }
    }

    return encodeInteger(count);
}

std::string CommandHandler::executeHSet(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 4) {
        return encodeError(
            "ERR wrong number of arguments for 'hset' command"
        );
    }

    if (
        !database_.hashSet(
            arguments[1],
            arguments[2],
            arguments[3]
        )
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(1);
}

std::string CommandHandler::executeHGet(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'hget' command"
        );
    }

    const storage::HashLookupResult result =
        database_.hashGet(
            arguments[1],
            arguments[2]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    if (
        result.status ==
        storage::LookupStatus::Missing
    ) {
        return encodeNullBulkString();
    }

    return encodeBulkString(
        result.value
    );
}

std::string CommandHandler::executeLPush(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'lpush' command"
        );
    }

    const auto result =
        database_.listPushLeft(
            arguments[1],
            arguments[2]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(
        static_cast<long long>(
            result.length
        )
    );
}

std::string CommandHandler::executeRPush(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'rpush' command"
        );
    }

    const auto result =
        database_.listPushRight(
            arguments[1],
            arguments[2]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(
        static_cast<long long>(
            result.length
        )
    );
}

std::string CommandHandler::executeLPop(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 2) {
        return encodeError(
            "ERR wrong number of arguments for 'lpop' command"
        );
    }

    const auto result =
        database_.listPopLeft(
            arguments[1]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    if (
        result.status ==
        storage::LookupStatus::Missing
    ) {
        return encodeNullBulkString();
    }

    return encodeBulkString(
        result.value
    );
}

std::string CommandHandler::executeRPop(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 2) {
        return encodeError(
            "ERR wrong number of arguments for 'rpop' command"
        );
    }

    const auto result =
        database_.listPopRight(
            arguments[1]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    if (
        result.status ==
        storage::LookupStatus::Missing
    ) {
        return encodeNullBulkString();
    }

    return encodeBulkString(
        result.value
    );
}

std::string CommandHandler::executeLRange(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 4) {
        return encodeError(
            "ERR wrong number of arguments for 'lrange' command"
        );
    }

    long long start = 0;
    long long stop = 0;

    if (
        !parseInteger(arguments[2], start) ||
        !parseInteger(arguments[3], stop)
    ) {
        return encodeError(
            "ERR value is not an integer or out of range"
        );
    }

    const auto result =
        database_.listRange(
            arguments[1],
            start,
            stop
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeArray(
        result.values
    );
}

std::string CommandHandler::executeLLen(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 2) {
        return encodeError(
            "ERR wrong number of arguments for 'llen' command"
        );
    }

    const auto result =
        database_.listLength(
            arguments[1]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(
        static_cast<long long>(
            result.length
        )
    );
}

std::string CommandHandler::executeSAdd(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'sadd' command"
        );
    }

    const auto result =
        database_.setAdd(
            arguments[1],
            arguments[2]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(
        result.changed ? 1 : 0
    );
}

std::string CommandHandler::executeSRem(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'srem' command"
        );
    }

    const auto result =
        database_.setRemove(
            arguments[1],
            arguments[2]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(
        result.changed ? 1 : 0
    );
}

std::string CommandHandler::executeSIsMember(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'sismember' command"
        );
    }

    const auto result =
        database_.setContains(
            arguments[1],
            arguments[2]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(
        result.is_member ? 1 : 0
    );
}

std::string CommandHandler::executeSMembers(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 2) {
        return encodeError(
            "ERR wrong number of arguments for 'smembers' command"
        );
    }

    const auto result =
        database_.setMembers(
            arguments[1]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeArray(
        result.values
    );
}

std::string CommandHandler::executeZAdd(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 4) {
        return encodeError(
            "ERR wrong number of arguments for 'zadd' command"
        );
    }

    double score = 0.0;

    if (
        !parseDouble(
            arguments[2],
            score
        )
    ) {
        return encodeError(
            "ERR value is not a valid float"
        );
    }

    const auto result =
        database_.sortedSetAdd(
            arguments[1],
            score,
            arguments[3]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeInteger(
        result.inserted ? 1 : 0
    );
}

std::string CommandHandler::executeZScore(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'zscore' command"
        );
    }

    const auto result =
        database_.sortedSetScore(
            arguments[1],
            arguments[2]
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    if (
        result.status ==
        storage::LookupStatus::Missing
    ) {
        return encodeNullBulkString();
    }

    return encodeBulkString(
        std::to_string(
            result.score
        )
    );
}

std::string CommandHandler::executeZRange(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 4) {
        return encodeError(
            "ERR wrong number of arguments for 'zrange' command"
        );
    }

    long long start = 0;
    long long stop = 0;

    if (
        !parseInteger(
            arguments[2],
            start
        ) ||
        !parseInteger(
            arguments[3],
            stop
        )
    ) {
        return encodeError(
            "ERR value is not an integer or out of range"
        );
    }

    const auto result =
        database_.sortedSetRange(
            arguments[1],
            start,
            stop
        );

    if (
        result.status ==
        storage::LookupStatus::WrongType
    ) {
        return encodeWrongTypeError();
    }

    return encodeArray(
        result.values
    );
}

std::string CommandHandler::executeExpire(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 3) {
        return encodeError(
            "ERR wrong number of arguments for 'expire' command"
        );
    }

    long long seconds = 0;

    if (
        !parseInteger(
            arguments[2],
            seconds
        )
    ) {
        return encodeError(
            "ERR value is not an integer or out of range"
        );
    }

    const bool success =
        database_.expire(
            arguments[1],
            seconds
        );

    return encodeInteger(
        success ? 1 : 0
    );
}

std::string CommandHandler::executeTTL(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 2) {
        return encodeError(
            "ERR wrong number of arguments for 'ttl' command"
        );
    }

    return encodeInteger(
        database_.ttl(
            arguments[1]
        )
    );
}

std::string CommandHandler::executeSave(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 1) {
        return encodeError(
            "ERR wrong number of arguments for 'save' command"
        );
    }

    if (!snapshot_.save(database_)) {
        return encodeError(
            "ERR failed to save snapshot"
        );
    }

    return encodeSimpleString(
        "OK"
    );
}

std::string CommandHandler::encodeSimpleString(
    const std::string& value
) {
    return "+" +
           value +
           "\r\n";
}

std::string CommandHandler::encodeBulkString(
    const std::string& value
) {
    return "$" +
           std::to_string(value.size()) +
           "\r\n" +
           value +
           "\r\n";
}

std::string CommandHandler::encodeNullBulkString() {
    return "$-1\r\n";
}

std::string CommandHandler::encodeInteger(
    long long value
) {
    return ":" +
           std::to_string(value) +
           "\r\n";
}

std::string CommandHandler::encodeRawArray(
    const std::vector<std::string>& responses
) {
    std::string output =
        "*" +
        std::to_string(
            responses.size()
        ) +
        "\r\n";

    for (
        const std::string& response :
        responses
    ) {
        output += response;
    }

    return output;
}

std::string CommandHandler::encodeArray(
    const std::vector<std::string>& values
) {
    std::string output =
        "*" +
        std::to_string(values.size()) +
        "\r\n";

    for (
        const std::string& value :
        values
    ) {
        output +=
            encodeBulkString(value);
    }

    return output;
}

std::string CommandHandler::encodeNullArray(){
    return "*-1\r\n";
}

std::string CommandHandler::encodeError(
    const std::string& message
) {
    return "-" +
           message +
           "\r\n";
}

std::string CommandHandler::encodeWrongTypeError() {
    return
        "-WRONGTYPE Operation against a key "
        "holding the wrong kind of value\r\n";
}

}  // namespace redis::command