#pragma once

#include "redis/protocol/resp_value.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <cstdint>

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
        std::size_t offset,
        std::size_t depth
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
        std::size_t offset,
        std::size_t depth
    ) const;

    [[nodiscard]]
    static std::size_t findCrlf(
        std::string_view input,
        std::size_t offset
    );

    static constexpr std::size_t
        kMaxBulkStringBytes=
            8*1024*1024;

    static constexpr std::int64_t
        kMaxArrayElements=
            1024;

    static constexpr std::size_t
        kMaxNestingDepth=
            32;

    static constexpr std::size_t
        kMaxLineBytes=
            64*1024;
};

}  // namespace redis::protocol