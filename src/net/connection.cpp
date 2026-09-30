#include "redis/net/connection.hpp"

#include <utility>
#include <unistd.h>

namespace redis::net {

namespace {

constexpr int kInvalidFileDescriptor = -1;

}  // namespace

Connection::Connection(int fd)
    : fd_(fd),
      input_buffer_(),
      output_buffer_(),
      output_offset_(0) {
}

Connection::~Connection() {
    closeSocket();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_),
      input_buffer_(std::move(other.input_buffer_)),
      output_buffer_(std::move(other.output_buffer_)),
      output_offset_(other.output_offset_) {
    other.fd_ = kInvalidFileDescriptor;
    other.output_offset_ = 0;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    closeSocket();

    fd_ = other.fd_;
    input_buffer_ = std::move(other.input_buffer_);
    output_buffer_ = std::move(other.output_buffer_);
    output_offset_ = other.output_offset_;

    other.fd_ = kInvalidFileDescriptor;
    other.output_offset_ = 0;

    return *this;
}

int Connection::fd() const noexcept {
    return fd_;
}

void Connection::appendInput(
    const char* data,
    std::size_t size
) {
    input_buffer_.append(data, size);
}

const std::string& Connection::inputBuffer() const noexcept {
    return input_buffer_;
}

void Connection::clearInput() {
    input_buffer_.clear();
}

void Connection::appendOutput(std::string_view data) {
    if (output_offset_ == output_buffer_.size()) {
        output_buffer_.clear();
        output_offset_ = 0;
    }

    output_buffer_.append(
        data.data(),
        data.size()
    );
}

bool Connection::hasPendingOutput() const noexcept {
    return output_offset_ < output_buffer_.size();
}

std::string_view Connection::pendingOutput() const noexcept {
    if (!hasPendingOutput()) {
        return {};
    }

    return std::string_view(
        output_buffer_.data() + output_offset_,
        output_buffer_.size() - output_offset_
    );
}

void Connection::consumeOutput(std::size_t bytes) {
    output_offset_ += bytes;

    if (output_offset_ >= output_buffer_.size()) {
        output_buffer_.clear();
        output_offset_ = 0;
    }
}

void Connection::closeSocket() noexcept {
    if (fd_ != kInvalidFileDescriptor) {
        ::close(fd_);
        fd_ = kInvalidFileDescriptor;
    }
}

}  // namespace redis::net