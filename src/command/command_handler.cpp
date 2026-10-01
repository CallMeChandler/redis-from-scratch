#include "redis/command/command_handler.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <variant>
#include <vector>

namespace redis::command {

std::string CommandHandler::execute(
    const protocol::RespValue& value
) const {
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

    if (command=="PING"){
        return executePing(arguments);
    }

    if (command=="ECHO"){
        return executeEcho(arguments);
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

    if (array==nullptr){
        return false;
    }

    arguments.clear();
    arguments.reserve(array->size());

    for (const protocol::RespValue& element:*array) {
        const protocol::BulkString* bulk_string =
            std::get_if<protocol::BulkString>(
                &element.value
            );

        if (bulk_string!=nullptr){
            arguments.push_back(
                bulk_string->value
            );

            continue;
        }

        const protocol::SimpleString* simple_string =
            std::get_if<protocol::SimpleString>(
                &element.value
            );

        if (simple_string!=nullptr){
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

std::string CommandHandler::executePing(
    const std::vector<std::string>& arguments
) {
    if (arguments.size() == 1){
        return encodeSimpleString(
            "PONG"
        );
    }

    if (arguments.size() == 2){
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
    if (arguments.size()!=2) {
        return encodeError(
            "ERR wrong number of arguments for 'echo' command"
        );
    }

    return encodeBulkString(
        arguments[1]
    );
}

std::string CommandHandler::encodeSimpleString(
    const std::string& value
) {
    return "+" + value + "\r\n";
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

std::string CommandHandler::encodeError(
    const std::string& message
) {
    return "-" + message + "\r\n";
}

}