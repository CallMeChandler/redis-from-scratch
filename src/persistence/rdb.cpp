#include "redis/persistence/rdb.hpp"

#include "redis/storage/redis_value.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

namespace redis::persistence {

namespace {

constexpr const char* kMagic =
    "REDISFS1";

enum class StoredType : std::uint8_t {
    String = 1,
    Hash = 2,
    List = 3,
    Set = 4,
    SortedSet = 5
};

void writeString(
    std::ofstream& output,
    const std::string& value
) {
    const std::uint64_t size =
        static_cast<std::uint64_t>(
            value.size()
        );

    output.write(
        reinterpret_cast<const char*>(
            &size
        ),
        sizeof(size)
    );

    output.write(
        value.data(),
        static_cast<std::streamsize>(
            value.size()
        )
    );
}

bool readString(
    std::ifstream& input,
    std::string& value
) {
    std::uint64_t size = 0;

    input.read(
        reinterpret_cast<char*>(
            &size
        ),
        sizeof(size)
    );

    if (!input) {
        return false;
    }

    value.resize(
        static_cast<std::size_t>(
            size
        )
    );

    input.read(
        value.data(),
        static_cast<std::streamsize>(
            size
        )
    );

    return static_cast<bool>(input);
}

}  // namespace

RdbSnapshot::RdbSnapshot(
    std::string path
)
    : path_(std::move(path)) {
}

bool RdbSnapshot::save(
    storage::Database& database
) const {
    std::ofstream output(
        path_,
        std::ios::binary |
        std::ios::trunc
    );

    if (!output) {
        return false;
    }

    output.write(
        kMagic,
        8
    );

    const auto& values =
        database.values();

    const std::uint64_t key_count =
        static_cast<std::uint64_t>(
            values.size()
        );

    output.write(
        reinterpret_cast<const char*>(
            &key_count
        ),
        sizeof(key_count)
    );

    for (
        const auto& entry :
        values
    ) {
        const std::string& key =
            entry.first;

        const storage::RedisValue& value =
            entry.second;

        const storage::RedisType type =
            value.type();

        StoredType stored_type;

        if (
            type ==
            storage::RedisType::String
        ) {
            stored_type =
                StoredType::String;
        } else if (
            type ==
            storage::RedisType::Hash
        ) {
            stored_type =
                StoredType::Hash;
        } else if (
            type ==
            storage::RedisType::List
        ) {
            stored_type =
                StoredType::List;
        } else if (
            type ==
            storage::RedisType::Set
        ) {
            stored_type =
                StoredType::Set;
        } else {
            stored_type =
                StoredType::SortedSet;
        }

        output.write(
            reinterpret_cast<const char*>(
                &stored_type
            ),
            sizeof(stored_type)
        );

        writeString(
            output,
            key
        );

        if (
            type ==
            storage::RedisType::String
        ) {
            writeString(
                output,
                value.asString()
            );
        } else if (
            type ==
            storage::RedisType::Hash
        ) {
            const auto& hash =
                value.asHash();

            const std::uint64_t count =
                hash.size();

            output.write(
                reinterpret_cast<const char*>(
                    &count
                ),
                sizeof(count)
            );

            for (
                const auto& field :
                hash
            ) {
                writeString(
                    output,
                    field.first
                );

                writeString(
                    output,
                    field.second
                );
            }
        } else if (
            type ==
            storage::RedisType::List
        ) {
            const auto& list =
                value.asList();

            const std::uint64_t count =
                list.size();

            output.write(
                reinterpret_cast<const char*>(
                    &count
                ),
                sizeof(count)
            );

            for (
                const std::string& item :
                list
            ) {
                writeString(
                    output,
                    item
                );
            }
        } else if (
            type ==
            storage::RedisType::Set
        ) {
            const auto& set =
                value.asSet();

            const std::uint64_t count =
                set.size();

            output.write(
                reinterpret_cast<const char*>(
                    &count
                ),
                sizeof(count)
            );

            for (
                const std::string& member :
                set
            ) {
                writeString(
                    output,
                    member
                );
            }
        } else {
            const auto& sorted_set =
                value.asSortedSet();

            const std::uint64_t count =
                sorted_set.size();

            output.write(
                reinterpret_cast<const char*>(
                    &count
                ),
                sizeof(count)
            );

            for (
                const auto& member :
                sorted_set
            ) {
                writeString(
                    output,
                    member.first
                );

                output.write(
                    reinterpret_cast<const char*>(
                        &member.second
                    ),
                    sizeof(member.second)
                );
            }
        }

        if (!output) {
            return false;
        }
    }

    return true;
}

bool RdbSnapshot::load(
    storage::Database& database
) const {
    std::ifstream input(
        path_,
        std::ios::binary
    );

    if (!input) {
        return true;
    }

    char magic[8];

    input.read(
        magic,
        8
    );

    if (!input) {
        return false;
    }

    if (
        std::string(
            magic,
            8
        ) != kMagic
    ) {
        return false;
    }

    std::uint64_t key_count = 0;

    input.read(
        reinterpret_cast<char*>(
            &key_count
        ),
        sizeof(key_count)
    );

    if (!input) {
        return false;
    }

    database.clear();

    for (
        std::uint64_t index = 0;
        index < key_count;
        ++index
    ) {
        StoredType type;

        input.read(
            reinterpret_cast<char*>(
                &type
            ),
            sizeof(type)
        );

        if (!input) {
            return false;
        }

        std::string key;

        if (!readString(input, key)) {
            return false;
        }

        if (
            type ==
            StoredType::String
        ) {
            std::string value;

            if (
                !readString(
                    input,
                    value
                )
            ) {
                return false;
            }

            database.restoreValue(
                std::move(key),
                storage::RedisValue{
                    std::move(value)
                }
            );
        } else if (
            type ==
            StoredType::Hash
        ) {
            std::uint64_t count = 0;

            input.read(
                reinterpret_cast<char*>(
                    &count
                ),
                sizeof(count)
            );

            storage::RedisHash hash;

            for (
                std::uint64_t i = 0;
                i < count;
                ++i
            ) {
                std::string field;
                std::string value;

                if (
                    !readString(
                        input,
                        field
                    ) ||
                    !readString(
                        input,
                        value
                    )
                ) {
                    return false;
                }

                hash.emplace(
                    std::move(field),
                    std::move(value)
                );
            }

            database.restoreValue(
                std::move(key),
                storage::RedisValue{
                    std::move(hash)
                }
            );
        } else if (
            type ==
            StoredType::List
        ) {
            std::uint64_t count = 0;

            input.read(
                reinterpret_cast<char*>(
                    &count
                ),
                sizeof(count)
            );

            storage::RedisList list;

            for (
                std::uint64_t i = 0;
                i < count;
                ++i
            ) {
                std::string item;

                if (
                    !readString(
                        input,
                        item
                    )
                ) {
                    return false;
                }

                list.push_back(
                    std::move(item)
                );
            }

            database.restoreValue(
                std::move(key),
                storage::RedisValue{
                    std::move(list)
                }
            );
        } else if (
            type ==
            StoredType::Set
        ) {
            std::uint64_t count = 0;

            input.read(
                reinterpret_cast<char*>(
                    &count
                ),
                sizeof(count)
            );

            storage::RedisSet set;

            for (
                std::uint64_t i = 0;
                i < count;
                ++i
            ) {
                std::string member;

                if (
                    !readString(
                        input,
                        member
                    )
                ) {
                    return false;
                }

                set.insert(
                    std::move(member)
                );
            }

            database.restoreValue(
                std::move(key),
                storage::RedisValue{
                    std::move(set)
                }
            );
        } else if (
            type ==
            StoredType::SortedSet
        ) {
            std::uint64_t count = 0;

            input.read(
                reinterpret_cast<char*>(
                    &count
                ),
                sizeof(count)
            );

            storage::RedisSortedSet sorted_set;

            for (
                std::uint64_t i = 0;
                i < count;
                ++i
            ) {
                std::string member;
                double score = 0.0;

                if (
                    !readString(
                        input,
                        member
                    )
                ) {
                    return false;
                }

                input.read(
                    reinterpret_cast<char*>(
                        &score
                    ),
                    sizeof(score)
                );

                if (!input) {
                    return false;
                }

                sorted_set.emplace(
                    std::move(member),
                    score
                );
            }

            database.restoreValue(
                std::move(key),
                storage::RedisValue{
                    std::move(sorted_set)
                }
            );
        } else {
            return false;
        }
    }

    return true;
}

}  // namespace redis::persistence