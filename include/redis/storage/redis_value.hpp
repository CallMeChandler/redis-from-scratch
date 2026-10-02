#pragma once

#include <string>
#include <utility>
#include <variant>

namespace redis::storage {

enum class RedisType {
    String
};


class RedisValue {
public:
    explicit RedisValue(std::string value)
        : value_(std::move(value)) {
    }

    [[nodiscard]]
    RedisType type() const noexcept {
        return RedisType::String;
    }

    [[nodiscard]]
    const std::string& asString() const {
        return std::get<std::string>(value_);
    }

private:
    std::variant<
        std::string
    > value_;
};

}