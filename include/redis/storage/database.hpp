#pragma once

#include "redis/storage/redis_value.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace redis::storage {

enum class LookupStatus {
    Found,
    Missing,
    WrongType
};

struct StringLookupResult {
    LookupStatus status;
    std::string value;
};

struct HashLookupResult {
    LookupStatus status;
    std::string value;
};

struct ListPushResult {
    LookupStatus status;
    std::size_t length;
};

struct ListPopResult {
    LookupStatus status;
    std::string value;
};

struct ListRangeResult {
    LookupStatus status;
    std::vector<std::string> values;
};

struct ListLengthResult {
    LookupStatus status;
    std::size_t length;
};

class Database {
public:
    void setString(
        std::string key,
        std::string value
    );

    [[nodiscard]]
    StringLookupResult getString(
        const std::string& key
    ) const;

    [[nodiscard]]
    bool exists(
        const std::string& key
    ) const;

    bool erase(
        const std::string& key
    );

    [[nodiscard]]
    std::size_t size() const noexcept;

    bool hashSet(
        const std::string& key,
        std::string field,
        std::string value
    );

    [[nodiscard]]
    HashLookupResult hashGet(
        const std::string& key,
        const std::string& field
    ) const;

    [[nodiscard]]
    ListPushResult listPushLeft(
        const std::string& key,
        std::string value
    );

    [[nodiscard]]
    ListPushResult listPushRight(
        const std::string& key,
        std::string value
    );

    [[nodiscard]]
    ListPopResult listPopLeft(
        const std::string& key
    );

    [[nodiscard]]
    ListPopResult listPopRight(
        const std::string& key
    );

    [[nodiscard]]
    ListRangeResult listRange(
        const std::string& key,
        long long start,
        long long stop
    ) const;

    [[nodiscard]]
    ListLengthResult listLength(
        const std::string& key
    ) const;

private:
    std::unordered_map<
        std::string,
        RedisValue
    > values_;
};

}  // namespace redis::storage