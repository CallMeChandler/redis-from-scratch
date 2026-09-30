#pragma once

#include "redis/protocol/resp_value.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace redis::protocol {

enum class ParseStatus {
    Complete,
    Incomplete,
    Error
};

struct ParseResult {
    ParseStatus status;
    RespValue value;
    std::size_t consumed;
    std::string error_message;
};

class RespParser {
public:
    [[nodiscard]]
    ParseResult parse(std::string_view input) const;

private:
    [[nodiscard]]
    ParseResult parseValue(
        std::string_view input,
        std::size_t offset
    ) const;

    [[nodiscard]]
    ParseResult parseSimpleString(
        std::string_view input,
        std::size_t offset
    ) const;

    [[nodiscard]]
    ParseResult parseError(
        std::string_view input,
        std::size_t offset
    ) const;

    [[nodiscard]]
    ParseResult parseInteger(
        std::string_view input,
        std::size_t offset
    ) const;

    [[nodiscard]]
    ParseResult parseBulkString(
        std::string_view input,
        std::size_t offset
    ) const;

    [[nodiscard]]
    ParseResult parseArray(
        std::string_view input,
        std::size_t offset
    ) const;

    [[nodiscard]]
    static std::size_t findCrlf(
        std::string_view input,
        std::size_t offset
    );
};

}  // namespace redis::protocol