#include "redis/config/server_config.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace redis::config {

namespace {

std::string trim(
    const std::string& input
) {
    const std::size_t first =
        input.find_first_not_of(
            " \t\r\n"
        );

    if (
        first ==
        std::string::npos
    ) {
        return {};
    }

    const std::size_t last =
        input.find_last_not_of(
            " \t\r\n"
        );

    return input.substr(
        first,
        last - first + 1
    );
}

unsigned long long parseUnsigned(
    const std::string& value,
    const std::string& key,
    std::size_t line_number
) {
    unsigned long long result = 0;

    const char* begin =
        value.data();

    const char* end =
        value.data() +
        value.size();

    const auto parse_result =
        std::from_chars(
            begin,
            end,
            result
        );

    if (
        parse_result.ec !=
            std::errc{} ||
        parse_result.ptr != end
    ) {
        throw std::runtime_error(
            "invalid numeric value for '" +
            key +
            "' on line " +
            std::to_string(
                line_number
            )
        );
    }

    return result;
}

bool parseBoolean(
    const std::string& value,
    const std::string& key,
    std::size_t line_number
) {
    if (
        value == "yes" ||
        value == "true" ||
        value == "1"
    ) {
        return true;
    }

    if (
        value == "no" ||
        value == "false" ||
        value == "0"
    ) {
        return false;
    }

    throw std::runtime_error(
        "invalid boolean value for '" +
        key +
        "' on line " +
        std::to_string(
            line_number
        )
    );
}

}  // namespace

ServerConfig ServerConfig::loadFromFile(
    const std::string& path
) {
    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error(
            "failed to open configuration file: " +
            path
        );
    }

    ServerConfig config;

    std::string line;
    std::size_t line_number = 0;

    while (
        std::getline(
            input,
            line
        )
    ) {
        ++line_number;

        const std::size_t comment =
            line.find('#');

        if (
            comment !=
            std::string::npos
        ) {
            line.erase(comment);
        }

        line = trim(line);

        if (line.empty()) {
            continue;
        }

        std::istringstream stream(
            line
        );

        std::string key;
        std::string value;
        std::string extra;

        stream >> key >> value;

        if (
            key.empty() ||
            value.empty()
        ) {
            throw std::runtime_error(
                "invalid configuration on line " +
                std::to_string(
                    line_number
                )
            );
        }

        if (stream >> extra) {
            throw std::runtime_error(
                "unexpected extra value on line " +
                std::to_string(
                    line_number
                )
            );
        }

        if (key == "port") {
            const auto parsed =
                parseUnsigned(
                    value,
                    key,
                    line_number
                );

            if (
                parsed == 0 ||
                parsed > 65535
            ) {
                throw std::runtime_error(
                    "port must be between 1 and 65535"
                );
            }

            config.port =
                static_cast<std::uint16_t>(
                    parsed
                );
        } else if (
            key == "bind"
        ) {
            config.bind_address =
                value;
        } else if (
            key == "rdb-file"
        ) {
            config.rdb_path =
                value;
        } else if (
            key == "aof-file"
        ) {
            config.aof_path =
                value;
        } else if (
            key == "listen-backlog"
        ) {
            const auto parsed =
                parseUnsigned(
                    value,
                    key,
                    line_number
                );

            if (
                parsed == 0 ||
                parsed >
                    static_cast<
                        unsigned long long
                    >(
                        std::numeric_limits<int>::max()
                    )
            ) {
                throw std::runtime_error(
                    "invalid listen-backlog"
                );
            }

            config.listen_backlog =
                static_cast<int>(
                    parsed
                );
        } else if (
            key == "max-clients"
        ) {
            const auto parsed =
                parseUnsigned(
                    value,
                    key,
                    line_number
                );

            if (parsed == 0) {
                throw std::runtime_error(
                    "max-clients must be greater than zero"
                );
            }

            config.max_clients =
                static_cast<std::size_t>(
                    parsed
                );
        } else if (
            key == "epoll-timeout-ms"
        ) {
            const auto parsed =
                parseUnsigned(
                    value,
                    key,
                    line_number
                );

            if (
                parsed >
                static_cast<
                    unsigned long long
                >(
                    std::numeric_limits<int>::max()
                )
            ) {
                throw std::runtime_error(
                    "invalid epoll-timeout-ms"
                );
            }

            config.epoll_timeout_ms =
                static_cast<int>(
                    parsed
                );
        } else if (
            key == "expire-batch-size"
        ) {
            const auto parsed =
                parseUnsigned(
                    value,
                    key,
                    line_number
                );

            if (parsed == 0) {
                throw std::runtime_error(
                    "expire-batch-size must be greater than zero"
                );
            }

            config.expiration_batch_size =
                static_cast<std::size_t>(
                    parsed
                );
        } else if (
            key == "save-on-shutdown"
        ) {
            config.save_on_shutdown =
                parseBoolean(
                    value,
                    key,
                    line_number
                );
        } else {
            throw std::runtime_error(
                "unknown configuration option '" +
                key +
                "' on line " +
                std::to_string(
                    line_number
                )
            );
        }
    }

    return config;
}

}  // namespace redis::config