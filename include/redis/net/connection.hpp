#pragma once

#include "redis/command/transaction_state.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace redis::net {

class Connection {
public:
    explicit Connection(int fd);

    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    [[nodiscard]]
    int fd() const noexcept;

    void appendInput(
        const char* data,
        std::size_t size
    );

    [[nodiscard]]
    const std::string& inputBuffer() const noexcept;

    void consumeInput(
        std::size_t bytes
    );

    void appendOutput(
        std::string_view data
    );

    [[nodiscard]]
    bool hasPendingOutput() const noexcept;

    [[nodiscard]]
    std::string_view pendingOutput() const noexcept;

    void consumeOutput(
        std::size_t bytes
    );

    [[nodiscard]]
    command::TransactionState& transaction() noexcept;

private:
    void closeSocket() noexcept;

    int fd_;

    std::string input_buffer_;
    std::string output_buffer_;

    std::size_t output_offset_;

    command::TransactionState transaction_;
};

}  // namespace redis::net