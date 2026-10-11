#include "redis/persistence/aof.hpp"

#include "redis/protocol/resp_parser.hpp"

#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace redis::persistence {

AppendOnlyFile::AppendOnlyFile(
    std::string path
)
    : path_(std::move(path)) {
}

bool AppendOnlyFile::append(
    const std::vector<std::string>& arguments
) {
    std::ofstream output(
        path_,
        std::ios::binary |
        std::ios::app
    );

    if (!output) {
        return false;
    }

    const std::string encoded =
        encodeCommand(arguments);

    output.write(
        encoded.data(),
        static_cast<std::streamsize>(
            encoded.size()
        )
    );

    output.flush();

    return static_cast<bool>(output);
}

bool AppendOnlyFile::load(
    std::vector<
        protocol::RespValue
    >& commands
) const {
    std::ifstream input(
        path_,
        std::ios::binary
    );

    if (!input) {
        return true;
    }

    const std::string data{
        std::istreambuf_iterator<char>(
            input
        ),
        std::istreambuf_iterator<char>()
    };

    protocol::RespParser parser;

    std::size_t offset = 0;

    while (
        offset <
        data.size()
    ) {
        const std::string_view remaining(
            data.data() + offset,
            data.size() - offset
        );

        const protocol::ParseResult result =
            parser.parse(
                remaining
            );

        if (
            result.status ==
            protocol::ParseStatus::Incomplete
        ) {
            return false;
        }

        if (
            result.status ==
            protocol::ParseStatus::Error
        ) {
            return false;
        }

        commands.push_back(
            result.value
        );

        offset +=
            result.consumed;
    }

    return true;
}

std::string AppendOnlyFile::encodeCommand(
    const std::vector<std::string>& arguments
) {
    std::string output =
        "*" +
        std::to_string(
            arguments.size()
        ) +
        "\r\n";

    for (
        const std::string& argument :
        arguments
    ) {
        output+=
            "$" +
            std::to_string(
                arguments.size()
            ) +
            "\r\n" +
            argument +
            "\r\n";
    }

    return output;
}

}