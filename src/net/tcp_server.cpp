#include "redis/net/tcp_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace redis::net {

namespace {

constexpr int kInvalidFileDescriptor = -1;
constexpr int kListenBacklog = 128;
constexpr int kMaxEvents = 64;
constexpr std::size_t kReadBufferSize = 4096;

std::runtime_error makeSystemError(
    const std::string& operation
) {
    return std::runtime_error(
        operation + " failed: " +
        std::strerror(errno)
    );
}

}  // namespace

TcpServer::TcpServer(std::uint16_t port)
    : port_(port),
      server_fd_(kInvalidFileDescriptor),
      epoll_fd_(kInvalidFileDescriptor),
      connections_(),
      resp_parser_(),
      command_handler_() {
}

TcpServer::~TcpServer() {
    connections_.clear();

    if (epoll_fd_ != kInvalidFileDescriptor) {
        ::close(epoll_fd_);
    }

    if (server_fd_ != kInvalidFileDescriptor) {
        ::close(server_fd_);
    }
}

void TcpServer::start() {
    createSocket();
    configureSocket();
    bindSocket();
    listenForConnections();

    createEpollInstance();
    registerServerSocket();

    std::cout
        << "Redis server listening on port "
        << port_
        << std::endl;

    runEventLoop();
}

void TcpServer::createSocket() {
    server_fd_ = ::socket(
        AF_INET,
        SOCK_STREAM |
            SOCK_NONBLOCK |
            SOCK_CLOEXEC,
        0
    );

    if (server_fd_ == kInvalidFileDescriptor) {
        throw makeSystemError("socket");
    }
}

void TcpServer::configureSocket() {
    constexpr int enable = 1;

    const int result = ::setsockopt(
        server_fd_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &enable,
        sizeof(enable)
    );

    if (result == -1) {
        throw makeSystemError("setsockopt");
    }
}

void TcpServer::bindSocket() {
    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr =
        htonl(INADDR_ANY);
    server_address.sin_port =
        htons(port_);

    const int result = ::bind(
        server_fd_,
        reinterpret_cast<const sockaddr*>(
            &server_address
        ),
        sizeof(server_address)
    );

    if (result == -1) {
        throw makeSystemError("bind");
    }
}

void TcpServer::listenForConnections() {
    const int result = ::listen(
        server_fd_,
        kListenBacklog
    );

    if (result == -1) {
        throw makeSystemError("listen");
    }
}

void TcpServer::createEpollInstance() {
    epoll_fd_ =
        ::epoll_create1(EPOLL_CLOEXEC);

    if (epoll_fd_ == kInvalidFileDescriptor) {
        throw makeSystemError(
            "epoll_create1"
        );
    }
}

void TcpServer::registerServerSocket() {
    addToEpoll(
        server_fd_,
        EPOLLIN
    );
}

void TcpServer::runEventLoop() {
    epoll_event events[kMaxEvents]{};

    while (true) {
        const int ready_count =
            ::epoll_wait(
                epoll_fd_,
                events,
                kMaxEvents,
                -1
            );

        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw makeSystemError(
                "epoll_wait"
            );
        }

        for (
            int index = 0;
            index < ready_count;
            ++index
        ) {
            const int ready_fd =
                events[index].data.fd;

            const std::uint32_t event_flags =
                events[index].events;

            if (ready_fd == server_fd_) {
                handleNewConnection();
                continue;
            }

            handleClientEvent(
                ready_fd,
                event_flags
            );
        }
    }
}

void TcpServer::handleNewConnection() {
    while (true) {
        sockaddr_in client_address{};

        socklen_t client_address_length =
            sizeof(client_address);

        const int client_fd =
            ::accept4(
                server_fd_,
                reinterpret_cast<sockaddr*>(
                    &client_address
                ),
                &client_address_length,
                SOCK_NONBLOCK |
                    SOCK_CLOEXEC
            );

        if (
            client_fd ==
            kInvalidFileDescriptor
        ) {
            if (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            ) {
                return;
            }

            if (errno == EINTR) {
                continue;
            }

            throw makeSystemError(
                "accept4"
            );
        }

        char client_ip[
            INET_ADDRSTRLEN
        ]{};

        const char* conversion_result =
            ::inet_ntop(
                AF_INET,
                &client_address.sin_addr,
                client_ip,
                sizeof(client_ip)
            );

        if (conversion_result == nullptr) {
            ::close(client_fd);

            throw makeSystemError(
                "inet_ntop"
            );
        }

        std::unique_ptr<Connection> connection =
            std::make_unique<Connection>(
                client_fd
            );

        connections_.emplace(
            client_fd,
            std::move(connection)
        );

        try {
            addToEpoll(
                client_fd,
                EPOLLIN |
                    EPOLLRDHUP
            );
        } catch (...) {
            connections_.erase(
                client_fd
            );

            throw;
        }

        std::cout
            << "Client connected from "
            << client_ip
            << ':'
            << ntohs(
                client_address.sin_port
            )
            << " on fd "
            << client_fd
            << std::endl;
    }
}

void TcpServer::handleClientEvent(
    int client_fd,
    std::uint32_t events
) {
    const auto connection_iterator =
        connections_.find(client_fd);

    if (
        connection_iterator ==
        connections_.end()
    ) {
        return;
    }

    Connection& connection =
        *connection_iterator->second;

    if ((events & EPOLLERR) != 0U) {
        std::cerr
            << "Socket error on fd "
            << client_fd
            << std::endl;

        removeClient(client_fd);
        return;
    }

    const bool peer_closed =
        (events & EPOLLRDHUP) != 0U ||
        (events & EPOLLHUP) != 0U;

    if ((events & EPOLLIN) != 0U) {
        const bool connection_alive =
            readFromClient(connection);

        if (!connection_alive) {
            removeClient(client_fd);
            return;
        }
    }

    if ((events & EPOLLOUT) != 0U) {
        const bool connection_alive =
            writeToClient(connection);

        if (!connection_alive) {
            removeClient(client_fd);
            return;
        }
    }

    if (peer_closed) {
        if (
            connection.hasPendingOutput()
        ) {
            const bool connection_alive =
                writeToClient(connection);

            if (!connection_alive) {
                removeClient(client_fd);
                return;
            }
        }

        if (
            !connection.hasPendingOutput()
        ) {
            removeClient(client_fd);
        }
    }
}

bool TcpServer::readFromClient(
    Connection& connection
) {
    char buffer[kReadBufferSize]{};

    bool peer_reached_eof = false;

    while (true) {
        const ssize_t bytes_read =
            ::recv(
                connection.fd(),
                buffer,
                sizeof(buffer),
                0
            );

        if (bytes_read > 0) {
            const std::size_t bytes_received =
                static_cast<std::size_t>(
                    bytes_read
                );

            connection.appendInput(
                buffer,
                bytes_received
            );

            std::cout
                << "Received "
                << bytes_received
                << " bytes from fd "
                << connection.fd()
                << std::endl;

            continue;
        }

        if (bytes_read == 0) {
            peer_reached_eof = true;
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        if (
            errno == EAGAIN ||
            errno == EWOULDBLOCK
        ) {
            break;
        }

        return false;
    }

    const bool protocol_valid =
        processInput(connection);

    if (!protocol_valid) {
        return false;
    }

    if (
        connection.hasPendingOutput()
    ) {
        const bool connection_alive =
            writeToClient(connection);

        if (!connection_alive) {
            return false;
        }
    }

    if (
        peer_reached_eof &&
        !connection.hasPendingOutput()
    ) {
        return false;
    }

    return true;
}

bool TcpServer::processInput(
    Connection& connection
) {
    while (
        !connection.inputBuffer().empty()
    ) {
        const protocol::ParseResult result =
            resp_parser_.parse(
                connection.inputBuffer()
            );

        if (
            result.status ==
            protocol::ParseStatus::Incomplete
        ) {
            return true;
        }

        if (
            result.status ==
            protocol::ParseStatus::Error
        ) {
            std::cerr
                << "RESP protocol error on fd "
                << connection.fd()
                << ": "
                << result.error_message
                << std::endl;

            connection.appendOutput(
                "-ERR Protocol error\r\n"
            );

            connection.consumeInput(
                connection.inputBuffer().size()
            );

            return true;
        }

        connection.consumeInput(
            result.consumed
        );

        const std::string response =
            command_handler_.execute(
                result.value
            );

        connection.appendOutput(
            response
        );

        std::cout
            << "Executed RESP command on fd "
            << connection.fd()
            << std::endl;
    }

    return true;
}

bool TcpServer::writeToClient(
    Connection& connection
) {
    while (
        connection.hasPendingOutput()
    ) {
        const std::string_view output =
            connection.pendingOutput();

        const ssize_t bytes_sent =
            ::send(
                connection.fd(),
                output.data(),
                output.size(),
                MSG_NOSIGNAL
            );

        if (bytes_sent > 0) {
            connection.consumeOutput(
                static_cast<std::size_t>(
                    bytes_sent
                )
            );

            continue;
        }

        if (
            bytes_sent == -1 &&
            errno == EINTR
        ) {
            continue;
        }

        if (
            bytes_sent == -1 &&
            (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            )
        ) {
            updateClientEvents(
                connection.fd(),
                EPOLLIN |
                    EPOLLOUT |
                    EPOLLRDHUP
            );

            return true;
        }

        return false;
    }

    updateClientEvents(
        connection.fd(),
        EPOLLIN |
            EPOLLRDHUP
    );

    return true;
}

void TcpServer::updateClientEvents(
    int client_fd,
    std::uint32_t events
) {
    epoll_event event{};

    event.events = events;
    event.data.fd = client_fd;

    const int result =
        ::epoll_ctl(
            epoll_fd_,
            EPOLL_CTL_MOD,
            client_fd,
            &event
        );

    if (result == -1) {
        throw makeSystemError(
            "epoll_ctl modify"
        );
    }
}

void TcpServer::addToEpoll(
    int fd,
    std::uint32_t events
) {
    epoll_event event{};

    event.events = events;
    event.data.fd = fd;

    const int result =
        ::epoll_ctl(
            epoll_fd_,
            EPOLL_CTL_ADD,
            fd,
            &event
        );

    if (result == -1) {
        throw makeSystemError(
            "epoll_ctl add"
        );
    }
}

void TcpServer::removeClient(
    int client_fd
) {
    const int result =
        ::epoll_ctl(
            epoll_fd_,
            EPOLL_CTL_DEL,
            client_fd,
            nullptr
        );

    if (
        result == -1 &&
        errno != ENOENT &&
        errno != EBADF
    ) {
        std::cerr
            << "epoll_ctl delete failed for fd "
            << client_fd
            << ": "
            << std::strerror(errno)
            << std::endl;
    }

    connections_.erase(
        client_fd
    );

    std::cout
        << "Client disconnected from fd "
        << client_fd
        << std::endl;
}

}  // namespace redis::net