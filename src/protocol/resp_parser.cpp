#include "redis/protocol/resp_parser.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace redis::protocol {

namespace {

constexpr std::size_t kNotFound =
    std::string_view::npos;

ParseResult makeIncomplete() {
    return ParseResult{
        ParseStatus::Incomplete,
        RespValue{},
        0,
        {}
    };
}

ParseResult makeError(
    std::string message
) {
    return ParseResult{
        ParseStatus::Error,
        RespValue{},
        0,
        std::move(message)
    };
}

ParseResult makeComplete(
    RespValue value,
    std::size_t consumed
) {
    return ParseResult{
        ParseStatus::Complete,
        std::move(value),
        consumed,
        {}
    };
}

bool parseIntegerValue(
    std::string_view text,
    std::int64_t& value
) {
    if (text.empty()) {
        return false;
    }

    const char* begin = text.data();
    const char* end =
        text.data() + text.size();

    const std::from_chars_result result =
        std::from_chars(
            begin,
            end,
            value
        );

    return
        result.ec == std::errc{} &&
        result.ptr == end;
}

}  // namespace

ParseResult RespParser::parse(
    std::string_view input
) const {
    if (input.empty()) {
        return makeIncomplete();
    }

    return parseValue(
        input,
        0
    );
}

ParseResult RespParser::parseValue(
    std::string_view input,
    std::size_t offset
) const {
    if (offset >= input.size()) {
        return makeIncomplete();
    }

    const char prefix =
        input[offset];

    switch (prefix) {
        case '+':
            return parseSimpleString(
                input,
                offset
            );

        case '-':
            return parseError(
                input,
                offset
            );

        case ':':
            return parseInteger(
                input,
                offset
            );

        case '$':
            return parseBulkString(
                input,
                offset
            );

        case '*':
            return parseArray(
                input,
                offset
            );

        default:
            return makeError(
                "unknown RESP type prefix"
            );
    }
}

ParseResult RespParser::parseSimpleString(
    std::string_view input,
    std::size_t offset
) const {
    const std::size_t line_end =
        findCrlf(
            input,
            offset + 1
        );

    if (line_end == kNotFound) {
        return makeIncomplete();
    }

    const std::string value(
        input.substr(
            offset + 1,
            line_end - offset - 1
        )
    );

    return makeComplete(
        RespValue{
            SimpleString{
                value
            }
        },
        line_end + 2 - offset
    );
}

ParseResult RespParser::parseError(
    std::string_view input,
    std::size_t offset
) const {
    const std::size_t line_end =
        findCrlf(
            input,
            offset + 1
        );

    if (line_end == kNotFound) {
        return makeIncomplete();
    }

    const std::string value(
        input.substr(
            offset + 1,
            line_end - offset - 1
        )
    );

    return makeComplete(
        RespValue{
            Error{
                value
            }
        },
        line_end + 2 - offset
    );
}

ParseResult RespParser::parseInteger(
    std::string_view input,
    std::size_t offset
) const {
    const std::size_t line_end =
        findCrlf(
            input,
            offset + 1
        );

    if (line_end == kNotFound) {
        return makeIncomplete();
    }

    const std::string_view number_text =
        input.substr(
            offset + 1,
            line_end - offset - 1
        );

    std::int64_t value = 0;

    if (
        !parseIntegerValue(
            number_text,
            value
        )
    ) {
        return makeError(
            "invalid RESP integer"
        );
    }

    return makeComplete(
        RespValue{
            value
        },
        line_end + 2 - offset
    );
}

ParseResult RespParser::parseBulkString(
    std::string_view input,
    std::size_t offset
) const {
    const std::size_t length_end =
        findCrlf(
            input,
            offset + 1
        );

    if (length_end == kNotFound) {
        return makeIncomplete();
    }

    const std::string_view length_text =
        input.substr(
            offset + 1,
            length_end - offset - 1
        );

    std::int64_t length = 0;

    if (
        !parseIntegerValue(
            length_text,
            length
        )
    ) {
        return makeError(
            "invalid bulk string length"
        );
    }

    if (length == -1) {
        return makeComplete(
            RespValue{
                NullBulkString{}
            },
            length_end + 2 - offset
        );
    }

    if (length < -1) {
        return makeError(
            "bulk string length cannot be less than -1"
        );
    }

    const std::uint64_t unsigned_length =
        static_cast<std::uint64_t>(
            length
        );

    if (
        unsigned_length >
        static_cast<std::uint64_t>(
            std::numeric_limits<
                std::size_t
            >::max()
        )
    ) {
        return makeError(
            "bulk string length is too large"
        );
    }

    const std::size_t payload_length =
        static_cast<std::size_t>(
            unsigned_length
        );

    const std::size_t payload_start =
        length_end + 2;

    if (
        payload_start > input.size()
    ) {
        return makeIncomplete();
    }

    if (
        payload_length >
        input.size() - payload_start
    ) {
        return makeIncomplete();
    }

    const std::size_t payload_end =
        payload_start +
        payload_length;

    if (
        input.size() <
        payload_end + 2
    ) {
        return makeIncomplete();
    }

    if (
        input[payload_end] != '\r' ||
        input[payload_end + 1] != '\n'
    ) {
        return makeError(
            "bulk string is not terminated by CRLF"
        );
    }

    const std::string value(
        input.substr(
            payload_start,
            payload_length
        )
    );

    return makeComplete(
        RespValue{
            BulkString{
                value
            }
        },
        payload_end + 2 - offset
    );
}

ParseResult RespParser::parseArray(
    std::string_view input,
    std::size_t offset
) const {
    const std::size_t count_end =
        findCrlf(
            input,
            offset + 1
        );

    if (count_end == kNotFound) {
        return makeIncomplete();
    }

    const std::string_view count_text =
        input.substr(
            offset + 1,
            count_end - offset - 1
        );

    std::int64_t count = 0;

    if (
        !parseIntegerValue(
            count_text,
            count
        )
    ) {
        return makeError(
            "invalid RESP array length"
        );
    }

    if (count < 0) {
        return makeError(
            "null arrays are not supported yet"
        );
    }

    RespArray values;

    values.reserve(
        static_cast<std::size_t>(
            count
        )
    );

    std::size_t current_offset =
        count_end + 2;

    for (
        std::int64_t index = 0;
        index < count;
        ++index
    ) {
        const ParseResult element_result =
            parseValue(
                input,
                current_offset
            );

        if (
            element_result.status ==
            ParseStatus::Incomplete
        ) {
            return makeIncomplete();
        }

        if (
            element_result.status ==
            ParseStatus::Error
        ) {
            return element_result;
        }

        values.push_back(
            element_result.value
        );

        current_offset +=
            element_result.consumed;
    }

    return makeComplete(
        RespValue{
            std::move(values)
        },
        current_offset - offset
    );
}

std::size_t RespParser::findCrlf(
    std::string_view input,
    std::size_t offset
) {
    return input.find(
        "\r\n",
        offset
    );
}

}  // namespace redis::protocol