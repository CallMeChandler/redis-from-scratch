#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace redis::storage {

using RedisHash =
    std::unordered_map<
        std::string,
        std::string
    >;

using RedisList =
    std::deque<std::string>;

using RedisSet =
    std::unordered_set<std::string>;

using RedisSortedSet =
    std::unordered_map<
        std::string,
        double
    >;

enum class RedisType {
    String,
    Hash,
    List,
    Set,
    SortedSet
};

class RedisValue {
public:
    explicit RedisValue(std::string value)
        : value_(std::move(value)) {
    }

    explicit RedisValue(RedisHash value)
        : value_(std::move(value)) {
    }

    explicit RedisValue(RedisList value)
        : value_(std::move(value)) {
    }

    explicit RedisValue(RedisSet value)
        : value_(std::move(value)) {
    }

    explicit RedisValue(RedisSortedSet value)
        : value_(std::move(value)) {
    }

    [[nodiscard]]
    RedisType type() const noexcept {
        if (
            std::holds_alternative<std::string>(
                value_
            )
        ) {
            return RedisType::String;
        }

        if (
            std::holds_alternative<RedisHash>(
                value_
            )
        ) {
            return RedisType::Hash;
        }

        if (
            std::holds_alternative<RedisList>(
                value_
            )
        ) {
            return RedisType::List;
        }

        if (
            std::holds_alternative<RedisSet>(
                value_
            )
        ) {
            return RedisType::Set;
        }

        if (
            std::holds_alternative<RedisSortedSet>(
                value_
            )
        ) {
            return RedisType::SortedSet;
        }

        return RedisType::String;
    }

    [[nodiscard]]
    const std::string& asString() const {
        return std::get<std::string>(
            value_
        );
    }

    [[nodiscard]]
    RedisHash& asHash() {
        return std::get<RedisHash>(
            value_
        );
    }

    [[nodiscard]]
    const RedisHash& asHash() const {
        return std::get<RedisHash>(
            value_
        );
    }

    [[nodiscard]]
    RedisList& asList() {
        return std::get<RedisList>(
            value_
        );
    }

    [[nodiscard]]
    const RedisList& asList() const {
        return std::get<RedisList>(
            value_
        );
    }

    [[nodiscard]]
    RedisSet& asSet() {
        return std::get<RedisSet>(
            value_
        );
    }

    [[nodiscard]]
    const RedisSet& asSet() const {
        return std::get<RedisSet>(
            value_
        );
    }

    [[nodiscard]]
    RedisSortedSet& asSortedSet() {
        return std::get<RedisSortedSet>(
            value_
        );
    }

    [[nodiscard]]
    const RedisSortedSet& asSortedSet() const {
        return std::get<RedisSortedSet>(
            value_
        );
    }

private:
    std::variant<
        std::string,
        RedisHash,
        RedisList,
        RedisSet,
        RedisSortedSet
    > value_;
};

}  // namespace redis::storage