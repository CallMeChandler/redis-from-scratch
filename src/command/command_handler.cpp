#include "redis/command/command_handler.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
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

    arguments.reserve(
        array->size()
    );

    for (
        const protocol::RespValue& element :
        *array
    ) {
        const protocol::BulkString* bulk_string =
            std::get_if<protocol::BulkString>(
                &element.value
            );

        if (bulk_string != nullptr) {
            arguments.push_back(
                bulk_string->value
            );

            continue;
        }

        const protocol::SimpleString* simple_string =
            std::get_if<protocol::SimpleString>(
                &element.value
            );

        if (simple_string != nullptr) {
            arguments.push_back(
                simple_string->value
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
    if (text.empty()) {
        return false;
    }

    const char* begin =
        text.data();

    const char* end =
        text.data() +
        text.size();

    const auto result =
        std::from_chars(
            begin,
            end,
            value
        );

    return result.ec ==
               std::errc{} &&
           result.ptr == end;
}

std::string CommandHandler::executePing(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() == 1) {
        return encodeSimpleString(
            "PONG"
        );
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

    return encodeSimpleString(
        "OK"
    );
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

    long long existing_keys = 0;

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
            ++existing_keys;
        }
    }

    return encodeInteger(
        existing_keys
    );
}

std::string CommandHandler::executeDel(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() < 2) {
        return encodeError(
            "ERR wrong number of arguments for 'del' command"
        );
    }

    long long deleted_keys = 0;

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
            ++deleted_keys;
        }
    }

    return encodeInteger(
        deleted_keys
    );
}

std::string CommandHandler::executeHSet(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() != 4) {
        return encodeError(
            "ERR wrong number of arguments for 'hset' command"
        );
    }

    const bool success =
        database_.hashSet(
            arguments[1],
            arguments[2],
            arguments[3]
        );

    if (!success) {
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

    const storage::ListPushResult result =
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

    const storage::ListPushResult result =
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

    const storage::ListPopResult result =
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

    const storage::ListPopResult result =
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

    const storage::ListRangeResult result =
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

    const storage::ListLengthResult result =
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
           std::to_string(
               value.size()
           ) +
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
        std::to_string(
            values.size()
        ) +
        "\r\n";

    for (
        const std::string& value :
        values
    ) {
        output +=
            encodeBulkString(
                value
            );
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