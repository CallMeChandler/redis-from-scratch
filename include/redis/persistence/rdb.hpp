#pragma once

#include "redis/storage/database.hpp"

#include <string>

namespace redis::persistence{
    
class RdbSnapshot{
public:
    explicit RdbSnapshot(
        std::string path
    );

    [[nodiscard]]
    bool save(
        storage::Database& database
    ) const;

    [[nodiscard]]
    bool load(
        storage::Database& database
    ) const;

private:
    std::string path_;
};

}