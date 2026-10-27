#include "redis/replication/replication_manager.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace redis::replication {

void ReplicationManager::addReplica(
    int client_fd
) {
    replicas_.insert(
        client_fd
    );
}

void ReplicationManager::removeReplica(
    int client_fd
) {
    replicas_.erase(
        client_fd
    );
}

const std::unordered_set<int>&
ReplicationManager::replicas() const noexcept {
    return replicas_;
}

bool ReplicationManager::isWriteCommand(
    const std::string& command
) {
    return
        command == "SET" ||
        command == "DEL" ||
        command == "HSET" ||
        command == "LPUSH" ||
        command == "RPUSH" ||
        command == "LPOP" ||
        command == "RPOP" ||
        command == "SADD" ||
        command == "SREM" ||
        command == "ZADD" ||
        command == "EXPIRE";
}

std::string ReplicationManager::encodeCommand(
    const std::vector<std::string>& arguments
) {
    std::string output;

    output += "*";
    output += std::to_string(
        arguments.size()
    );
    output += "\r\n";

    for (
        const std::string& argument :
        arguments
    ) {
        output += "$";

        output += std::to_string(
            argument.size()
        );

        output += "\r\n";

        output += argument;

        output += "\r\n";
    }

    return output;
}

std::string ReplicationManager::buildFullSync(
    storage::Database& database
) {
    std::string output;

    /*
     * Marker telling the replica to discard its
     * existing local dataset and start a full sync.
     */
    output += "+FULLRESYNC\r\n";

    std::vector<std::string> keys;

    const auto& values =
        database.values();

    keys.reserve(
        values.size()
    );

    for (
        const auto& entry :
        values
    ) {
        keys.push_back(
            entry.first
        );
    }

    for (
        const std::string& key :
        keys
    ) {
        /*
         * Calling ttl() also lets lazy expiration
         * remove an already-expired key.
         */
        const long long ttl =
            database.ttl(
                key
            );

        if (ttl == -2) {
            continue;
        }

        const auto& current_values =
            database.values();

        const auto iterator =
            current_values.find(
                key
            );

        if (
            iterator ==
            current_values.end()
        ) {
            continue;
        }

        const storage::RedisValue& value =
            iterator->second;

        if (
            value.type() ==
            storage::RedisType::String
        ) {
            output +=
                encodeCommand({
                    "SET",
                    key,
                    value.asString()
                });
        }

        else if (
            value.type() ==
            storage::RedisType::Hash
        ) {
            const storage::RedisHash& hash =
                value.asHash();

            for (
                const auto& entry :
                hash
            ) {
                output +=
                    encodeCommand({
                        "HSET",
                        key,
                        entry.first,
                        entry.second
                    });
            }
        }

        else if (
            value.type() ==
            storage::RedisType::List
        ) {
            const storage::RedisList& list =
                value.asList();

            for (
                const std::string& item :
                list
            ) {
                output +=
                    encodeCommand({
                        "RPUSH",
                        key,
                        item
                    });
            }
        }

        else if (
            value.type() ==
            storage::RedisType::Set
        ) {
            const storage::RedisSet& set =
                value.asSet();

            for (
                const std::string& member :
                set
            ) {
                output +=
                    encodeCommand({
                        "SADD",
                        key,
                        member
                    });
            }
        }

        else if (
            value.type() ==
            storage::RedisType::SortedSet
        ) {
            const storage::RedisSortedSet& sorted_set =
                value.asSortedSet();

            for (
                const auto& entry :
                sorted_set
            ) {
                std::ostringstream score_stream;

                score_stream
                    << std::setprecision(17)
                    << entry.second;

                output +=
                    encodeCommand({
                        "ZADD",
                        key,
                        score_stream.str(),
                        entry.first
                    });
            }
        }

        /*
         * Restore remaining TTL after restoring
         * the underlying value.
         */
        if (ttl >= 0) {
            output +=
                encodeCommand({
                    "EXPIRE",
                    key,
                    std::to_string(ttl)
                });
        }
    }

    output += "+FULLSYNCEND\r\n";

    return output;
}

}  // namespace redis::replication