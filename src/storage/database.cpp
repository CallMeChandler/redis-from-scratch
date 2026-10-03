#include "redis/storage/database.hpp"

#include <string>
#include <utility>

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

}  // namespace redis::storage