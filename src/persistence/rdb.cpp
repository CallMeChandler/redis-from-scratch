#include "redis/persistence/rdb.hpp"

#include "redis/storage/redis_value.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace redis::persistence {

namespace {

constexpr const char* kMagicV1 =
    "REDISFS1";

constexpr const char* kMagicV2 =
    "REDISFS2";

constexpr std::int64_t kNoExpiration =
    -1;

enum class StoredType : std::uint8_t {
    String = 1,
    Hash = 2,
    List = 3,
    Set = 4,
    SortedSet = 5
};

struct SnapshotEntry {
    std::string key;
    std::int64_t expiration_unix_seconds;
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

std::int64_t currentUnixSeconds() {
    const auto now =
        std::chrono::system_clock::now();

    return std::chrono::duration_cast<
        std::chrono::seconds
    >(
        now.time_since_epoch()
    ).count();
}

bool writeValue(
    std::ofstream& output,
    const storage::RedisValue& value
) {
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
        const storage::RedisHash& hash =
            value.asHash();

        const std::uint64_t count =
            static_cast<std::uint64_t>(
                hash.size()
            );

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
        const storage::RedisList& list =
            value.asList();

        const std::uint64_t count =
            static_cast<std::uint64_t>(
                list.size()
            );

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
        const storage::RedisSet& set =
            value.asSet();

        const std::uint64_t count =
            static_cast<std::uint64_t>(
                set.size()
            );

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
        const storage::RedisSortedSet& sorted_set =
            value.asSortedSet();

        const std::uint64_t count =
            static_cast<std::uint64_t>(
                sorted_set.size()
            );

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

    return static_cast<bool>(output);
}

bool readValue(
    std::ifstream& input,
    StoredType type,
    storage::RedisValue& value
) {
    if (
        type ==
        StoredType::String
    ) {
        std::string string_value;

        if (
            !readString(
                input,
                string_value
            )
        ) {
            return false;
        }

        value =
            storage::RedisValue{
                std::move(string_value)
            };

        return true;
    }

    if (
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

        if (!input) {
            return false;
        }

        storage::RedisHash hash;

        for (
            std::uint64_t index = 0;
            index < count;
            ++index
        ) {
            std::string field;
            std::string field_value;

            if (
                !readString(
                    input,
                    field
                ) ||
                !readString(
                    input,
                    field_value
                )
            ) {
                return false;
            }

            hash.emplace(
                std::move(field),
                std::move(field_value)
            );
        }

        value =
            storage::RedisValue{
                std::move(hash)
            };

        return true;
    }

    if (
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

        if (!input) {
            return false;
        }

        storage::RedisList list;

        for (
            std::uint64_t index = 0;
            index < count;
            ++index
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

        value =
            storage::RedisValue{
                std::move(list)
            };

        return true;
    }

    if (
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

        if (!input) {
            return false;
        }

        storage::RedisSet set;

        for (
            std::uint64_t index = 0;
            index < count;
            ++index
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

        value =
            storage::RedisValue{
                std::move(set)
            };

        return true;
    }

    if (
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

        if (!input) {
            return false;
        }

        storage::RedisSortedSet sorted_set;

        for (
            std::uint64_t index = 0;
            index < count;
            ++index
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

        value =
            storage::RedisValue{
                std::move(sorted_set)
            };

        return true;
    }

    return false;
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
    /*
     * First copy the key names.
     *
     * Database::ttl() performs lazy expiration and may erase an
     * expired key. Iterating values_ while calling ttl() directly
     * could therefore invalidate iterators.
     */
    std::vector<std::string> keys;

    {
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
    }

    /*
     * Build a list containing only keys that still logically exist.
     */
    std::vector<SnapshotEntry> entries;

    entries.reserve(
        keys.size()
    );

    const std::int64_t now =
        currentUnixSeconds();

    for (
        const std::string& key :
        keys
    ) {
        const long long remaining_ttl =
            database.ttl(key);

        /*
         * -2 means the key no longer exists.
         *
         * This can happen because ttl() lazily removed it.
         */
        if (remaining_ttl == -2) {
            continue;
        }

        std::int64_t expiration =
            kNoExpiration;

        if (remaining_ttl >= 0) {
            expiration =
                now +
                static_cast<std::int64_t>(
                    remaining_ttl
                );
        }

        entries.push_back(
            SnapshotEntry{
                key,
                expiration
            }
        );
    }

    std::ofstream output(
        path_,
        std::ios::binary |
        std::ios::trunc
    );

    if (!output) {
        return false;
    }

    /*
     * Version 2 includes expiration metadata.
     */
    output.write(
        kMagicV2,
        8
    );

    const std::uint64_t key_count =
        static_cast<std::uint64_t>(
            entries.size()
        );

    output.write(
        reinterpret_cast<const char*>(
            &key_count
        ),
        sizeof(key_count)
    );

    if (!output) {
        return false;
    }

    for (
        const SnapshotEntry& entry :
        entries
    ) {
        /*
         * ttl() may have modified values_, so look the value up again
         * rather than holding an old iterator.
         */
        const auto& values =
            database.values();

        const auto iterator =
            values.find(
                entry.key
            );

        if (
            iterator ==
            values.end()
        ) {
            return false;
        }

        const storage::RedisValue& value =
            iterator->second;

        /*
         * Store type first.
         *
         * writeValue() handles the type byte and payload.
         */
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
            entry.key
        );

        /*
         * New in REDISFS2:
         *
         * -1 = permanent key
         * otherwise absolute Unix expiration time
         */
        output.write(
            reinterpret_cast<const char*>(
                &entry.expiration_unix_seconds
            ),
            sizeof(
                entry.expiration_unix_seconds
            )
        );

        /*
         * Serialize only the type-specific payload here.
         *
         * We already wrote the type above, so we cannot call
         * writeValue() because it would write the type twice.
         */
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
            const storage::RedisHash& hash =
                value.asHash();

            const std::uint64_t count =
                static_cast<std::uint64_t>(
                    hash.size()
                );

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
            const storage::RedisList& list =
                value.asList();

            const std::uint64_t count =
                static_cast<std::uint64_t>(
                    list.size()
                );

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
            const storage::RedisSet& set =
                value.asSet();

            const std::uint64_t count =
                static_cast<std::uint64_t>(
                    set.size()
                );

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
            const storage::RedisSortedSet& sorted_set =
                value.asSortedSet();

            const std::uint64_t count =
                static_cast<std::uint64_t>(
                    sorted_set.size()
                );

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

    /*
     * No snapshot yet is not an error.
     */
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

    const std::string version(
        magic,
        8
    );

    const bool is_v1 =
        version == kMagicV1;

    const bool is_v2 =
        version == kMagicV2;

    if (
        !is_v1 &&
        !is_v2
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

    const std::int64_t now =
        currentUnixSeconds();

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

        if (
            !readString(
                input,
                key
            )
        ) {
            return false;
        }

        std::int64_t expiration =
            kNoExpiration;

        /*
         * Version 1 snapshots did not contain TTL metadata.
         *
         * Loading old snapshots therefore restores their keys as
         * permanent keys.
         */
        if (is_v2) {
            input.read(
                reinterpret_cast<char*>(
                    &expiration
                ),
                sizeof(expiration)
            );

            if (!input) {
                return false;
            }
        }

        /*
         * Read the type-specific value.
         *
         * RedisValue has no default constructor, so handle each
         * type directly.
         */
        if (
            type ==
            StoredType::String
        ) {
            std::string string_value;

            if (
                !readString(
                    input,
                    string_value
                )
            ) {
                return false;
            }

            /*
             * Skip values that expired while the server was offline.
             */
            if (
                expiration !=
                    kNoExpiration &&
                expiration <= now
            ) {
                continue;
            }

            database.restoreValue(
                key,
                storage::RedisValue{
                    std::move(
                        string_value
                    )
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

            if (!input) {
                return false;
            }

            storage::RedisHash hash;

            for (
                std::uint64_t i = 0;
                i < count;
                ++i
            ) {
                std::string field;
                std::string field_value;

                if (
                    !readString(
                        input,
                        field
                    ) ||
                    !readString(
                        input,
                        field_value
                    )
                ) {
                    return false;
                }

                hash.emplace(
                    std::move(field),
                    std::move(field_value)
                );
            }

            if (
                expiration !=
                    kNoExpiration &&
                expiration <= now
            ) {
                continue;
            }

            database.restoreValue(
                key,
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

            if (!input) {
                return false;
            }

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

            if (
                expiration !=
                    kNoExpiration &&
                expiration <= now
            ) {
                continue;
            }

            database.restoreValue(
                key,
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

            if (!input) {
                return false;
            }

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

            if (
                expiration !=
                    kNoExpiration &&
                expiration <= now
            ) {
                continue;
            }

            database.restoreValue(
                key,
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

            if (!input) {
                return false;
            }

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

            if (
                expiration !=
                    kNoExpiration &&
                expiration <= now
            ) {
                continue;
            }

            database.restoreValue(
                key,
                storage::RedisValue{
                    std::move(
                        sorted_set
                    )
                }
            );
        } else {
            return false;
        }

        /*
         * Restore the TTL after restoring the value.
         */
        if (
            expiration !=
            kNoExpiration
        ) {
            const std::int64_t remaining =
                expiration - now;

            if (remaining > 0) {
                database.expire(
                    key,
                    static_cast<long long>(
                        remaining
                    )
                );
            }
        }
    }

    return true;
}

}  // namespace redis::persistence