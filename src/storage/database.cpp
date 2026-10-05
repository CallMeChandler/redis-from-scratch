#include "redis/storage/database.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace redis::storage {

void Database::setString(
    std::string key,
    std::string value
) {
    RedisValue redis_value(
        std::move(value)
    );

    values_.insert_or_assign(
        std::move(key),
        std::move(redis_value)
    );
}

StringLookupResult Database::getString(
    const std::string& key
) const {
    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return StringLookupResult{
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::String
    ) {
        return StringLookupResult{
            LookupStatus::WrongType,
            {}
        };
    }

    return StringLookupResult{
        LookupStatus::Found,
        iterator->second.asString()
    };
}

bool Database::exists(
    const std::string& key
) const {
    return values_.find(key) !=
           values_.end();
}

bool Database::erase(
    const std::string& key
) {
    return values_.erase(key) > 0;
}

std::size_t Database::size() const noexcept {
    return values_.size();
}

bool Database::hashSet(
    const std::string& key,
    std::string field,
    std::string value
) {
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

    return true;
}

HashLookupResult Database::hashGet(
    const std::string& key,
    const std::string& field
) const {
    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return HashLookupResult{
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Hash
    ) {
        return HashLookupResult{
            LookupStatus::WrongType,
            {}
        };
    }

    const RedisHash& hash =
        iterator->second.asHash();

    const auto field_iterator =
        hash.find(field);

    if (field_iterator == hash.end()) {
        return HashLookupResult{
            LookupStatus::Missing,
            {}
        };
    }

    return HashLookupResult{
        LookupStatus::Found,
        field_iterator->second
    };
}

ListPushResult Database::listPushLeft(
    const std::string& key,
    std::string value
) {
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

        return ListPushResult{
            LookupStatus::Found,
            1
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return ListPushResult{
            LookupStatus::WrongType,
            0
        };
    }

    RedisList& list =
        iterator->second.asList();

    list.push_front(
        std::move(value)
    );

    return ListPushResult{
        LookupStatus::Found,
        list.size()
    };
}

ListPushResult Database::listPushRight(
    const std::string& key,
    std::string value
) {
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

        return ListPushResult{
            LookupStatus::Found,
            1
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return ListPushResult{
            LookupStatus::WrongType,
            0
        };
    }

    RedisList& list =
        iterator->second.asList();

    list.push_back(
        std::move(value)
    );

    return ListPushResult{
        LookupStatus::Found,
        list.size()
    };
}

ListPopResult Database::listPopLeft(
    const std::string& key
) {
    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return ListPopResult{
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return ListPopResult{
            LookupStatus::WrongType,
            {}
        };
    }

    RedisList& list =
        iterator->second.asList();

    if (list.empty()) {
        values_.erase(iterator);

        return ListPopResult{
            LookupStatus::Missing,
            {}
        };
    }

    std::string value =
        std::move(list.front());

    list.pop_front();

    if (list.empty()) {
        values_.erase(iterator);
    }

    return ListPopResult{
        LookupStatus::Found,
        std::move(value)
    };
}

ListPopResult Database::listPopRight(
    const std::string& key
) {
    auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return ListPopResult{
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return ListPopResult{
            LookupStatus::WrongType,
            {}
        };
    }

    RedisList& list =
        iterator->second.asList();

    if (list.empty()) {
        values_.erase(iterator);

        return ListPopResult{
            LookupStatus::Missing,
            {}
        };
    }

    std::string value =
        std::move(list.back());

    list.pop_back();

    if (list.empty()) {
        values_.erase(iterator);
    }

    return ListPopResult{
        LookupStatus::Found,
        std::move(value)
    };
}

ListRangeResult Database::listRange(
    const std::string& key,
    long long start,
    long long stop
) const {
    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return ListRangeResult{
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return ListRangeResult{
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
        return ListRangeResult{
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
        return ListRangeResult{
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

    return ListRangeResult{
        LookupStatus::Found,
        std::move(result)
    };
}

ListLengthResult Database::listLength(
    const std::string& key
) const {
    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return ListLengthResult{
            LookupStatus::Missing,
            0
        };
    }

    if (
        iterator->second.type() !=
        RedisType::List
    ) {
        return ListLengthResult{
            LookupStatus::WrongType,
            0
        };
    }

    return ListLengthResult{
        LookupStatus::Found,
        iterator->second
            .asList()
            .size()
    };
}

SetMutationResult Database::setAdd(
    const std::string& key,
    std::string member
) {
    auto iterator =
        values_.find(key);

    if (iterator==values_.end()){
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

        return SetMutationResult{
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

    return SetMutationResult{
        LookupStatus::Found,
        result.second
    };
}

SetMutationResult Database::setRemove(
    const std::string& key,
    const std::string& member
) {
    auto iterator =
        values_.find(key);

    if (iterator==values_.end()){
        return SetMutationResult{
            LookupStatus::Missing,
            false
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Set
    ) {
        return SetMutationResult{
            LookupStatus::WrongType,
            false
        };
    }

    RedisSet& set =
        iterator->second.asSet();

    const bool removed =
        set.erase(member) > 0;

    if (set.empty()) {
        values_.erase(iterator);
    }

    return SetMutationResult{
        LookupStatus::Found,
        removed
    };
}

SetMembershipResult Database::setContains(
    const std::string& key,
    const std::string& member
) const {
    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()){
        return SetMembershipResult{
            LookupStatus::Missing,
            false
        };
    }

    if (
        iterator->second.type() != RedisType::Set
    ) {
        return SetMembershipResult{
            LookupStatus::WrongType,
            false
        };
    }

    const RedisSet& set =
        iterator->second.asSet();

    return SetMembershipResult{
        LookupStatus::Found,
        set.find(member) !=
            set.end()
    };
}

SetMembersResult Database::setMembers(
    const std::string& key
) const {
    const auto iterator =
        values_.find(key);

    if (iterator==values_.end()){
        return SetMembersResult{
            LookupStatus::Missing,
            {}
        };
    }

    if (
        iterator->second.type() !=
        RedisType::Set
    ) {
        return SetMembersResult{
            LookupStatus::WrongType,
            {}
        };
    }

    const RedisSet& set =
        iterator->second.asSet();

    std::vector<std::string> values;

    values.reserve(
        set.size()
    );

    for (
        const std::string& member :
        set
    ) {
        values.push_back(
            member
        );
    }

    return SetMembersResult{
        LookupStatus::Found,
        std::move(values)
    };
}

}  // namespace redis::storage