#pragma once

#include "redis/storage/redis_value.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace redis::storage {

class Database {
public:
    void setString(
        std::string key,
        std::string value
    );

    [[nodiscard]]
    std::optional<std::string> getString(
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

private:
    std::unordered_map<
        std::string,
        RedisValue
    > values_;
};

}  // namespace redis::storage