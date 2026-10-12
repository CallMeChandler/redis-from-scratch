#include "redis/persistence/aof.hpp"

#include "redis/protocol/resp_parser.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
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
    std::vector<protocol::RespValue>& commands
) const {
    commands.clear();

    std::ifstream input(
        path_,
        std::ios::binary
    );

    if (!input) {
        return true;
    }

    const std::string data(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );

    if (data.empty()) {
        return true;
    }

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

        if (
            result.consumed == 0 ||
            result.consumed >
                remaining.size()
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

bool AppendOnlyFile::existsAndNotEmpty() const {
    std::error_code error;

    const bool exists =
        std::filesystem::exists(
            path_,
            error
        );

    if (
        error ||
        !exists
    ) {
        return false;
    }

    const std::uintmax_t size =
        std::filesystem::file_size(
            path_,
            error
        );

    if (error) {
        return false;
    }

    return size > 0;
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
        output +=
            "$" +
            std::to_string(
                argument.size()
            ) +
            "\r\n";

        output += argument;
        output += "\r\n";
    }

    return output;
}

}  // namespace redis::persistence