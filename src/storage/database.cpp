#include "redis/storage/database.hpp"

#include <optional>
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

std::optional<std::string>
Database::getString(
    const std::string& key
) const {
    const auto iterator =
        values_.find(key);

    if (iterator == values_.end()) {
        return std::nullopt;
    }

    return iterator->second.asString();
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

}  // namespace redis::storage