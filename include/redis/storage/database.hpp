#pragma once

#include "redis/storage/redis_value.hpp"

#include <chrono>
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

struct SetMutationResult {
    LookupStatus status;
    bool changed;
};

struct SetMembershipResult {
    LookupStatus status;
    bool is_member;
};

struct SetMembersResult {
    LookupStatus status;
    std::vector<std::string> values;
};

struct SortedSetAddResult {
    LookupStatus status;
    bool inserted;
};

struct SortedSetScoreResult {
    LookupStatus status;
    double score;
};

struct SortedSetRangeResult {
    LookupStatus status;
    std::vector<std::string> values;
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
    );

    [[nodiscard]]
    bool exists(
        const std::string& key
    );

    bool erase(
        const std::string& key
    );

    [[nodiscard]]
    std::size_t size();

    bool hashSet(
        const std::string& key,
        std::string field,
        std::string value
    );

    [[nodiscard]]
    HashLookupResult hashGet(
        const std::string& key,
        const std::string& field
    );

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
    );

    [[nodiscard]]
    ListLengthResult listLength(
        const std::string& key
    );

    [[nodiscard]]
    SetMutationResult setAdd(
        const std::string& key,
        std::string member
    );

    [[nodiscard]]
    SetMutationResult setRemove(
        const std::string& key,
        const std::string& member
    );

    [[nodiscard]]
    SetMembershipResult setContains(
        const std::string& key,
        const std::string& member
    );

    [[nodiscard]]
    SetMembersResult setMembers(
        const std::string& key
    );

    [[nodiscard]]
    SortedSetAddResult sortedSetAdd(
        const std::string& key,
        double score,
        std::string member
    );

    [[nodiscard]]
    SortedSetScoreResult sortedSetScore(
        const std::string& key,
        const std::string& member
    );

    [[nodiscard]]
    SortedSetRangeResult sortedSetRange(
        const std::string& key,
        long long start,
        long long stop
    );

    bool expire(
        const std::string& key,
        long long seconds
    );

    [[nodiscard]]
    long long ttl(
        const std::string& key
    );

private:
    using Clock =
        std::chrono::steady_clock;

    bool removeIfExpired(
        const std::string& key
    );

    void removeExpiration(
        const std::string& key
    );

    std::unordered_map<
        std::string,
        RedisValue
    > values_;

    std::unordered_map<
        std::string,
        Clock::time_point
    > expirations_;
};

}  // namespace redis::storage