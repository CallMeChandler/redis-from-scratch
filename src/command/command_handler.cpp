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
    storage::Database& database
)
    : database_(database) {
}

std::string CommandHandler::execute(
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