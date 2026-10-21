#pragma once

#include "redis/protocol/resp_value.hpp"
#include "redis/storage/database.hpp"

#include <string>
#include <vector>

namespace redis::persistence {

class AppendOnlyFile {
public:
    explicit AppendOnlyFile(
        std::string path
    );

    [[nodiscard]]
    bool append(
        const std::vector<std::string>& arguments
    );

    [[nodiscard]]
    bool load(
        std::vector<
            protocol::RespValue
        >& commands
    ) const;

    [[nodiscard]]
    bool existsAndNotEmpty() const;

    [[nodiscard]]
    bool rewrite(
        storage::Database& database
    );

private:
    [[nodiscard]]
    static std::string encodeCommand(
        const std::vector<std::string>& arguments
    );

    std::string path_;
};

}  // namespace redis::persistence