#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

namespace redis::storage {

using RedisHash =
    std::unordered_map<
        std::string,
        std::string
    >;

enum class RedisType {
    String,
    Hash
};

class RedisValue {
public:
    explicit RedisValue(std::string value)
        : value_(std::move(value)) {
    }

    explicit RedisValue(RedisHash value)
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

        return RedisType::Hash;
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

private:
    std::variant<
        std::string,
        RedisHash
    > value_;
};

}  // namespace redis::storage