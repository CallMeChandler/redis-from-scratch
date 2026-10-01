#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace redis::storage {

class StringStore {
public:
    void set(
        std::string key,
        std::string value
    );

    [[nodiscard]]
    std::optional<std::string> get(
        const std::string& key
    ) const;

private:
    std::unordered_map<
        std::string,
        std::string
    > values_;
};

}