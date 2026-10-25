#include "redis/storage/database.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>
#include <limits>
#include <optional>

namespace redis::storage {

void Database::setString(
    std::string key,
    std::string value
) {
    removeIfExpired(key);

    RedisValue redis_value(
        std::move(value)
    );

    values_.insert_or_assign(
        key,
        std::move(redis_value)
    );

    markKeyModified(key);

    removeExpiration(key);
}

StringLookupResult Database::getString(
    const std::string& key
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::String
    ) {
        return {
            LookupStatus::WrongType,
            {}
        };
    }

    return {
        LookupStatus::Found,
        iterator->second.asString()
    };
}

bool Database::exists(
    const std::string& key
) {
    removeIfExpired(key);

    return values_.find(key) !=
           values_.end();
}

bool Database::erase(
    const std::string& key
) {
    removeIfExpired(key);

    const bool removed =
        values_.erase(key) > 0;

    if (removed) {
        markKeyModified(key);
    }

    removeExpiration(key);

    return removed;
}

std::size_t Database::size() {
    for (
        auto iterator = expirations_.begin();
        iterator != expirations_.end();
    ) {
        const std::string key =
            iterator->first;

        ++iterator;

        removeIfExpired(key);
    }

    return values_.size();
}

bool Database::hashSet(
    const std::string& key,
    std::string field,
    std::string value
) {
    removeIfExpired(key);

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        RedisHash hash;

        hash.emplace(
            std::move(field),
            std::move(value)
        );

        values_.emplace(
            key,
            RedisValue{
                std::move(hash)
            }
        );

        markKeyModified(key);

        return true;
    }

    if (
        iterator->second.type() !=
        RedisType::Hash
    ) {
        return false;
    }

    RedisHash& hash =
        iterator->second.asHash();

    hash.insert_or_assign(
        std::move(field),
        std::move(value)
    );

    markKeyModified(key);

    return true;
}

HashLookupResult Database::hashGet(
    const std::string& key,
    const std::string& field
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Hash
    ) {
        return {
            LookupStatus::WrongType,
            {}
        };
    }

    const RedisHash& hash =
        iterator->second.asHash();

    const auto field_iterator =
        hash.find(field);

    if (
        field_iterator ==
        hash.end()
    ) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    return {
        LookupStatus::Found,
        field_iterator->second
    };
}

ListPushResult Database::listPushLeft(
    const std::string& key,
    std::string value
) {
    removeIfExpired(key);

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        RedisList list;

        list.push_front(
            std::move(value)
        );

        values_.emplace(
            key,
            RedisValue{
                std::move(list)
            }
        );

        markKeyModified(key);

        return {
            LookupStatus::Found,
            1
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return {
            LookupStatus::WrongType,
            0
        };
    }

    RedisList& list =
        iterator->second.asList();

    list.push_front(
        std::move(value)
    );

    markKeyModified(key);

    return {
        LookupStatus::Found,
        list.size()
    };
}

ListPushResult Database::listPushRight(
    const std::string& key,
    std::string value
) {
    removeIfExpired(key);

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        RedisList list;

        list.push_back(
            std::move(value)
        );

        values_.emplace(
            key,
            RedisValue{
                std::move(list)
            }
        );

        markKeyModified(key);

        return {
            LookupStatus::Found,
            1
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return {
            LookupStatus::WrongType,
            0
        };
    }

    RedisList& list =
        iterator->second.asList();

    list.push_back(
        std::move(value)
    );

    markKeyModified(key);

    return {
        LookupStatus::Found,
        list.size()
    };
}

ListPopResult Database::listPopLeft(
    const std::string& key
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return {
            LookupStatus::WrongType,
            {}
        };
    }

    RedisList& list =
        iterator->second.asList();

    std::string value =
        std::move(list.front());

    list.pop_front();

    markKeyModified(key);

    if (list.empty()) {
        values_.erase(iterator);
        removeExpiration(key);
    }

    return {
        LookupStatus::Found,
        std::move(value)
    };
}

ListPopResult Database::listPopRight(
    const std::string& key
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return {
            LookupStatus::WrongType,
            {}
        };
    }

    RedisList& list =
        iterator->second.asList();

    std::string value =
        std::move(list.back());

    list.pop_back();

    markKeyModified(key);

    if (list.empty()) {
        values_.erase(iterator);
        removeExpiration(key);
    }

    return {
        LookupStatus::Found,
        std::move(value)
    };
}

ListRangeResult Database::listRange(
    const std::string& key,
    long long start,
    long long stop
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return {
            LookupStatus::WrongType,
            {}
        };
    }

    const RedisList& list =
        iterator->second.asList();

    const long long length =
        static_cast<long long>(
            list.size()
        );

    if (length == 0) {
        return {
            LookupStatus::Found,
            {}
        };
    }

    if (start < 0) {
        start = length + start;
    }

    if (stop < 0) {
        stop = length + stop;
    }

    start = std::max(
        0LL,
        start
    );

    stop = std::min(
        length - 1,
        stop
    );

    if (
        start >= length ||
        stop < 0 ||
        start > stop
    ) {
        return {
            LookupStatus::Found,
            {}
        };
    }

    std::vector<std::string> result;

    result.reserve(
        static_cast<std::size_t>(
            stop - start + 1
        )
    );

    for (
        long long index = start;
        index <= stop;
        ++index
    ) {
        result.push_back(
            list[
                static_cast<std::size_t>(
                    index
                )
            ]
        );
    }

    return {
        LookupStatus::Found,
        std::move(result)
    };
}

ListLengthResult Database::listLength(
    const std::string& key
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            0
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            0
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return {
            LookupStatus::WrongType,
            0
        };
    }

    return {
        LookupStatus::Found,
        iterator->second.asList().size()
    };
}

SetMutationResult Database::setAdd(
    const std::string& key,
    std::string member
) {
    removeIfExpired(key);

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        RedisSet set;

        set.insert(
            std::move(member)
        );

        values_.emplace(
            key,
            RedisValue{
                std::move(set)
            }
        );

        markKeyModified(key);

        return {
            LookupStatus::Found,
            true
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Set
    ) {
        return {
            LookupStatus::WrongType,
            false
        };
    }

    RedisSet& set =
        iterator->second.asSet();

    const auto result =
        set.insert(
            std::move(member)
        );

    if (result.second) {
        markKeyModified(key);
    }

    return {
        LookupStatus::Found,
        result.second
    };
}

SetMutationResult Database::setRemove(
    const std::string& key,
    const std::string& member
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            false
        };
    }

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            false
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Set
    ) {
        return {
            LookupStatus::WrongType,
            false
        };
    }

    RedisSet& set =
        iterator->second.asSet();

    const bool removed =
        set.erase(member) > 0;

    if (removed) {
        markKeyModified(key);
    }

    if (set.empty()) {
        values_.erase(iterator);
        removeExpiration(key);
    }

    return {
        LookupStatus::Found,
        removed
    };
}

SetMembershipResult Database::setContains(
    const std::string& key,
    const std::string& member
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            false
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            false
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Set
    ) {
        return {
            LookupStatus::WrongType,
            false
        };
    }

    const RedisSet& set =
        iterator->second.asSet();

    return {
        LookupStatus::Found,
        set.find(member) !=
            set.end()
    };
}

SetMembersResult Database::setMembers(
    const std::string& key
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Set
    ) {
        return {
            LookupStatus::WrongType,
            {}
        };
    }

    const RedisSet& set =
        iterator->second.asSet();

    std::vector<std::string> result;

    result.reserve(
        set.size()
    );

    for (
        const std::string& member :
        set
    ) {
        result.push_back(
            member
        );
    }

    return {
        LookupStatus::Found,
        std::move(result)
    };
}

SortedSetAddResult Database::sortedSetAdd(
    const std::string& key,
    double score,
    std::string member
) {
    removeIfExpired(key);

    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        RedisSortedSet sorted_set;

        sorted_set.emplace(
            std::move(member),
            score
        );

        values_.emplace(
            key,
            RedisValue{
                std::move(sorted_set)
            }
        );

        markKeyModified(key);

        return {
            LookupStatus::Found,
            true
        };
    }

    if (
        iterator->second.type() !=
        RedisType::SortedSet
    ) {
        return {
            LookupStatus::WrongType,
            false
        };
    }

    RedisSortedSet& sorted_set =
        iterator->second.asSortedSet();

    const auto existing =
        sorted_set.find(member);

    const bool inserted =
        existing ==
        sorted_set.end();

    sorted_set.insert_or_assign(
        std::move(member),
        score
    );

    markKeyModified(key);

    return {
        LookupStatus::Found,
        inserted
    };
}

SortedSetScoreResult Database::sortedSetScore(
    const std::string& key,
    const std::string& member
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            0.0
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            0.0
        };
    }

    if (
        iterator->second.type() !=
        RedisType::SortedSet
    ) {
        return {
            LookupStatus::WrongType,
            0.0
        };
    }

    const RedisSortedSet& sorted_set =
        iterator->second.asSortedSet();

    const auto member_iterator =
        sorted_set.find(member);

    if (
        member_iterator ==
        sorted_set.end()
    ) {
        return {
            LookupStatus::Missing,
            0.0
        };
    }

    return {
        LookupStatus::Found,
        member_iterator->second
    };
}

SortedSetRangeResult Database::sortedSetRange(
    const std::string& key,
    long long start,
    long long stop
) {
    if (removeIfExpired(key)) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return {
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::SortedSet
    ) {
        return {
            LookupStatus::WrongType,
            {}
        };
    }

    const RedisSortedSet& sorted_set =
        iterator->second.asSortedSet();

    std::vector<
        std::pair<std::string, double>
    > ordered;

    ordered.reserve(
        sorted_set.size()
    );

    for (
        const auto& entry :
        sorted_set
    ) {
        ordered.push_back(entry);
    }

    std::sort(
        ordered.begin(),
        ordered.end(),
        [](
            const auto& left,
            const auto& right
        ) {
            if (
                left.second ==
                right.second
            ) {
                return left.first <
                       right.first;
            }

            return left.second <
                   right.second;
        }
    );

    const long long length =
        static_cast<long long>(
            ordered.size()
        );

    if (length == 0) {
        return {
            LookupStatus::Found,
            {}
        };
    }

    if (start < 0) {
        start = length + start;
    }

    if (stop < 0) {
        stop = length + stop;
    }

    start = std::max(
        0LL,
        start
    );

    stop = std::min(
        length - 1,
        stop
    );

    if (
        start >= length ||
        stop < 0 ||
        start > stop
    ) {
        return {
            LookupStatus::Found,
            {}
        };
    }

    std::vector<std::string> result;

    for (
        long long index = start;
        index <= stop;
        ++index
    ) {
        result.push_back(
            ordered[
                static_cast<std::size_t>(
                    index
                )
            ].first
        );
    }

    return {
        LookupStatus::Found,
        std::move(result)
    };
}

bool Database::expire(
    const std::string& key,
    long long seconds
) {
    removeIfExpired(key);

    if (
        values_.find(key) ==
        values_.end()
    ) {
        return false;
    }

    if (seconds <= 0) {
        values_.erase(key);
        last_access_.erase(key);
        markKeyModified(key);
        removeExpiration(key);
        return true;
    }

    expirations_.insert_or_assign(
        key,
        Clock::now() +
            std::chrono::seconds(
                seconds
            )
    );

    markKeyModified(key);

    return true;
}

long long Database::ttl(
    const std::string& key
) {
    if (removeIfExpired(key)) {
        return -2;
    }

    if (
        values_.find(key) ==
        values_.end()
    ) {
        return -2;
    }

    const auto expiration_iterator =
        expirations_.find(key);

    if (
        expiration_iterator ==
        expirations_.end()
    ) {
        return -1;
    }

    const auto remaining =
        std::chrono::duration_cast<
            std::chrono::seconds
        >(
            expiration_iterator->second -
            Clock::now()
        ).count();

    if (remaining < 0) {
        values_.erase(key);
        last_access_.erase(key);
        expirations_.erase(
            expiration_iterator
        );

        return -2;
    }

    return remaining;
}

std::size_t Database::activeExpireCycle(
    std::size_t max_keys
) {
    if (
        max_keys == 0 ||
        expirations_.empty()
    ) {
        return 0;
    }

    const auto now =
        Clock::now();

    std::size_t checked = 0;
    std::size_t removed = 0;

    auto iterator =
        expirations_.begin();

    while(
        iterator != expirations_.end() &&
        checked < max_keys
    ) {
        ++checked;

        if (
            now >= iterator->second
        ) {
            const std::string key =
                iterator->first;

            values_.erase(key);
            last_access_.erase(key);

            markKeyModified(key);

            iterator =
                expirations_.erase(
                    iterator
                );

            ++removed;

            continue;
    }

        ++iterator;
    }

    return removed;
}

bool Database::removeIfExpired(
    const std::string& key
) {
    const auto expiration_iterator =
        expirations_.find(key);

    if (
        expiration_iterator ==
        expirations_.end()
    ) {
        return false;
    }

    if (
        Clock::now() <
        expiration_iterator->second
    ) {
        return false;
    }

    values_.erase(key);
    last_access_.erase(key);

    markKeyModified(key);

    expirations_.erase(
        expiration_iterator
    );

    return true;
}

void Database::removeExpiration(
    const std::string& key
) {
    expirations_.erase(key);
}

std::uint64_t Database::keyVersion(
    const std::string& key
) {
    removeIfExpired(key);

    const auto iterator =
        key_versions_.find(key);

    if (
        iterator ==
        key_versions_.end()
    ) {
        return 0;
    }

    return iterator->second;
}

void Database::markKeyModified(
    const std::string& key
) {
    ++key_versions_[key];
}

const std::unordered_map<
    std::string,
    RedisValue
>& Database::values() const noexcept {
    return values_;
}

void Database::clear() {
    values_.clear();
    expirations_.clear();
    key_versions_.clear();
}

void Database::restoreValue(
    std::string key,
    RedisValue value
) {

    touchKey(key);
    
    values_.insert_or_assign(
        std::move(key),
        std::move(value)
    );
    
}

std::size_t Database::estimatedMemoryUsage() {
    std::size_t total = 0;

    std::vector<std::string> keys;

    keys.reserve(
        values_.size()
    );

    for (
        const auto& entry:
        values_
    ) {
        keys.push_back(
            entry.first
        );
    }

    for (
        const std::string& key:
        keys
    ) {
        const std::size_t usage =
            estimatedMemoryUsage(
                key
            );

        total+=usage;
    }

    return total;
}

std::size_t Database::estimatedMemoryUsage(
    const std::string& key
) {
    removeIfExpired(
        key
    );

    const auto iterator =
        values_.find(
            key
        );

    if (
        iterator==
        values_.end()
    ) {
        return 0;
    }

    constexpr std::size_t kKeyOverhead =
        32;

    std::size_t total =
        kKeyOverhead;

    total+=key.size();

    total+=iterator->second.estimatedMemoryUsage();

    if (
        expirations_.find(key) !=
        expirations_.end()
    ) {
        total+= sizeof(
            Clock::time_point
        );

        total+=16;
    }

    return total;
}

void Database::setMaxMemory(
    std::size_t bytes
) {
    max_memory_bytes_ =
        bytes;
}

std::size_t Database::maxMemory() const noexcept {
    return max_memory_bytes_;
}

void Database::touchKey(
    const std::string& key
) {
    removeIfExpired(
        key
    );

    if (
        values_.find(key) ==
        values_.end()
    ) {
        return;
    }

    ++access_clock_;

    last_access_[key]=
        access_clock_;
}

std::optional<std::string>
Database::evictLeastRecentlyUsed() {
    if (values_.empty()) {
        return std::nullopt;
    }

    auto candidate =
        values_.begin();

    std::uint64_t oldest_access =
        std::numeric_limits<
            std::uint64_t
        >::max();

    for (
        auto iterator =
            values_.begin();
        iterator != values_.end();
        ++iterator
    ) {
        const auto access_iterator =
            last_access_.find(
                iterator->first
            );

        const std::uint64_t access =
            access_iterator ==
                    last_access_.end()
                ? 0
                : access_iterator->second;

        if (
            access <
            oldest_access
        ) {
            oldest_access =
                access;

            candidate =
                iterator;
        }
    }

    const std::string key =
        candidate->first;

    values_.erase(
        candidate
    );

    expirations_.erase(
        key
    );

    last_access_.erase(
        key
    );

    markKeyModified(
        key
    );

    return key;
}

std::vector<std::string>
Database::evictToMemoryLimit() {
    std::vector<std::string> evicted_keys;

    if (
        max_memory_bytes_ == 0
    ) {
        return evicted_keys;
    }

    while (
        estimatedMemoryUsage() >
        max_memory_bytes_
    ) {
        const std::optional<std::string>
            evicted =
                evictLeastRecentlyUsed();

        if (!evicted.has_value()) {
            break;
        }

        evicted_keys.push_back(
            *evicted
        );
    }

    return evicted_keys;
}

}  // namespace redis::storage9