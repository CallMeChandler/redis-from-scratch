#include "redis/storage/string_store.hpp"

#include <optional>
#include <string>
#include <utility>

namespace redis::storage {

void StringStore::set(
    std::string key,
    std::string value
) {
    values_[std::move(key)] =
        std::move(value);
}

std::optional<std::string>
StringStore::get(
    const std::string& key
) const {
    const auto iterator =
        values_.find(key);

    if (iterator==values_.end()){
        return std::nullopt;
    }

    return iterator->second;
}

}