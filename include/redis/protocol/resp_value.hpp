#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace redis::protocol {

struct SimpleString {
    std::string value;
};

struct Error {
    std::string value;
};

struct BulkString {
    std::string value;
};

struct NullBulkString {
};

struct RespValue;

using RespArray = std::vector<RespValue>;

struct RespValue {
    using Storage = std::variant<
        SimpleString,
        Error,
        std::int64_t,
        BulkString,
        NullBulkString,
        RespArray
    >;

    RespValue()
        : value(SimpleString{}) {
    }

    RespValue(SimpleString simple_string)
        : value(std::move(simple_string)) {
    }

    RespValue(Error error)
        : value(std::move(error)) {
    }

    RespValue(std::int64_t integer)
        : value(integer) {
    }

    RespValue(BulkString bulk_string)
        : value(std::move(bulk_string)) {
    }

    RespValue(NullBulkString null_bulk_string)
        : value(null_bulk_string) {
    }

    RespValue(RespArray array)
        : value(std::move(array)) {
    }

    Storage value;
};

}  // namespace redis::protocol