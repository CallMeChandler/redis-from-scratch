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
#include <iomanip>
#include <sstream>

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

bool AppendOnlyFile::rewrite(
    storage::Database& database
) {
    const std::string temporary_path =
        path_ + ".tmp";

    std::ofstream output(
        temporary_path,
        std::ios::binary |
        std::ios::trunc
    );

    if (!output){
        return false;
    }
    
    std::vector<std::string> keys;

    {
        const auto& values =
            database.values();
        
        keys.reserve(
            values.size()
        );

        for (
            const auto& entry:
            values
        ) {
            keys.push_back(
                entry.first
            );
        }
    }

    for (
        const std::string& key:
        keys
    ){
        const long long ttl =
            database.ttl(key);

        if (ttl==-2){
            continue;
        }

        const auto& values =
            database.values();

        const auto iterator =
            values.find(key);

        if (
            iterator ==
            values.end()
        ) {
            continue;
        }

        const storage::RedisValue& value =
            iterator->second;

        if (
            value.type()==
            storage::RedisType::String
        ) {
            const std::vector<std::string> command{
                "SET",
                key,
                value.asString()
            };

            const std::string encoded =
                encodeCommand(command);

            output.write(
                encoded.data(),
                static_cast<std::streamsize>(
                    encoded.size()
                )
            );
        } else if (
            value.type() ==
            storage::RedisType::Hash
        ) {
            const storage::RedisHash& hash =
                value.asHash();

            for (
                const auto& entry:
                hash
            ) {
                const std::vector<std::string> command{
                    "HSET",
                    key,
                    entry.first,
                    entry.second
                };

                const std::string encoded =
                    encodeCommand(command);

                output.write(
                    encoded.data(),
                    static_cast<std::streamsize>(
                        encoded.size()
                    )
                );
            }
        } else if (
            value.type() == 
            storage::RedisType::List
        ) {
            const storage::RedisList& list =
                value.asList();

            for (
                const std::string& item:
                list
            ) {
                const std::vector<std::string> command{
                    "RPUSH",
                    key,
                    item
                };

                const std::string encoded =
                    encodeCommand(command);

                output.write(
                    encoded.data(),
                    static_cast<std::streamsize>(
                        encoded.size()
                    )
                );
            }
        } else if (
            value.type()==
            storage::RedisType::Set
        ) {
            const storage::RedisSet& set =
                value.asSet();

            for (
                const std::string& member:
                set
            ){
                const std::vector<std::string> command{
                    "SADD",
                    key,
                    member
                };

                const std::string encoded =
                    encodeCommand(command);

                output.write(
                    encoded.data(),
                    static_cast<std::streamsize>(
                        encoded.size()
                    )
                );
            }
        } else if (
            value.type()==
            storage::RedisType::SortedSet
        ) {
            const storage::RedisSortedSet& sorted_set =
                value.asSortedSet();

            for (
                const auto& entry:
                sorted_set
            ) {
                std::ostringstream score_stream;

                score_stream
                    << std::setprecision(17)
                    << entry.second;

                const std::vector<std::string> command{
                    "ZADD",
                    key,
                    score_stream.str(),
                    entry.first
                };

                const std::string encoded =
                    encodeCommand(command);

                output.write(
                    encoded.data(),
                    static_cast<std::streamsize>(
                        encoded.size()
                    )
                );
            }
        }

        //Recreate expiration after recreating the value
        //ttl==-1 means permanent
        //ttl>=0 means expiration exists

        if (ttl>=0){
            const std::vector<std::string> expire_command{
                "EXPIRE",
                key,
                std::to_string(ttl)
            };

            const std::string encoded =
                encodeCommand(
                    expire_command
                );

            output.write(
                encoded.data(),
                static_cast<std::streamsize>(
                    encoded.size()
                )
            );
        }

        if (!output){
            output.close();

            std::error_code remove_error;

            std::filesystem::remove(
                temporary_path,
                remove_error
            );

            return false;
        }
    }

    output.flush();

    if (!output){
        output.close();

        std::error_code remove_error;

        std::filesystem::remove(
            temporary_path,
            remove_error
        );

        return false;
    }

    output.close();

    std::error_code rename_error;

    std::filesystem::rename(
        temporary_path,
        path_,
        rename_error
    );

    if (rename_error){
        std::error_code remove_error;

        std::filesystem::remove(
            temporary_path,
            remove_error
        );

        return false;
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