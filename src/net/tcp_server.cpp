#include "redis/net/tcp_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace redis::net {

namespace {

constexpr int kInvalidFileDescriptor = -1;
constexpr int kListenBacklog = 128;
constexpr int kMaxEvents = 64;
constexpr std::size_t kBufferSize = 4096;

std::runtime_error makeSystemError(const std::string& operation) {
    return std::runtime_error(
        operation + " failed: " + std::strerror(errno)
    );
}

}  // namespace

TcpServer::TcpServer(std::uint16_t port)
    : port_(port),
      server_fd_(kInvalidFileDescriptor),
      epoll_fd_(kInvalidFileDescriptor) {
}

TcpServer::~TcpServer() {
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

    std::cout << "Redis server listening on port "
              << port_
              << std::endl;

    runEventLoop();
}

void TcpServer::createSocket() {
    server_fd_ = ::socket(
        AF_INET,
        SOCK_STREAM | SOCK_NONBLOCK,
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
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port_);

    const int result = ::bind(
        server_fd_,
        reinterpret_cast<const sockaddr*>(&server_address),
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
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);

    if (epoll_fd_ == kInvalidFileDescriptor) {
        throw makeSystemError("epoll_create1");
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
        const int ready_count = ::epoll_wait(
            epoll_fd_,
            events,
            kMaxEvents,
            -1
        );

        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw makeSystemError("epoll_wait");
        }

        for (int index = 0; index < ready_count; ++index) {
            const int ready_fd = events[index].data.fd;
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

        const int client_fd = ::accept4(
            server_fd_,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_address_length,
            SOCK_NONBLOCK | SOCK_CLOEXEC
        );

        if (client_fd == kInvalidFileDescriptor) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            if (errno == EINTR) {
                continue;
            }

            throw makeSystemError("accept4");
        }

        char client_ip[INET_ADDRSTRLEN]{};

        const char* conversion_result = ::inet_ntop(
            AF_INET,
            &client_address.sin_addr,
            client_ip,
            sizeof(client_ip)
        );

        if (conversion_result == nullptr) {
            ::close(client_fd);
            throw makeSystemError("inet_ntop");
        }

        addToEpoll(
            client_fd,
            EPOLLIN | EPOLLRDHUP
        );

        std::cout << "Client connected from "
                  << client_ip
                  << ':'
                  << ntohs(client_address.sin_port)
                  << " on fd "
                  << client_fd
                  << std::endl;
    }
}

void TcpServer::handleClientEvent(
    int client_fd,
    std::uint32_t events
) {
    if ((events & EPOLLERR) != 0U) {
        std::cerr << "Socket error on fd "
                  << client_fd
                  << std::endl;

        removeClient(client_fd);
        return;
    }

    const bool peer_closed =
        (events & EPOLLRDHUP) != 0U ||
        (events & EPOLLHUP) != 0U;

    if ((events & EPOLLIN) != 0U) {
        char buffer[kBufferSize]{};

        while (true) {
            const ssize_t bytes_read = ::recv(
                client_fd,
                buffer,
                sizeof(buffer),
                0
            );

            if (bytes_read > 0) {
                std::cout << "Received "
                          << bytes_read
                          << " bytes from fd "
                          << client_fd
                          << std::endl;

                std::size_t total_bytes_sent = 0;

                while (
                    total_bytes_sent <
                    static_cast<std::size_t>(bytes_read)
                ) {
                    const ssize_t bytes_sent = ::send(
                        client_fd,
                        buffer + total_bytes_sent,
                        static_cast<std::size_t>(bytes_read) -
                            total_bytes_sent,
                        MSG_NOSIGNAL
                    );

                    if (bytes_sent > 0) {
                        total_bytes_sent +=
                            static_cast<std::size_t>(
                                bytes_sent
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
                        std::cerr
                            << "Socket send buffer is full for fd "
                            << client_fd
                            << std::endl;

                        removeClient(client_fd);
                        return;
                    }

                    removeClient(client_fd);
                    return;
                }

                continue;
            }

            if (bytes_read == 0) {
                removeClient(client_fd);
                return;
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

            removeClient(client_fd);
            return;
        }
    }

    if (peer_closed) {
        removeClient(client_fd);
    }
}

void TcpServer::addToEpoll(
    int fd,
    std::uint32_t events
) {
    epoll_event event{};

    event.events = events;
    event.data.fd = fd;

    const int result = ::epoll_ctl(
        epoll_fd_,
        EPOLL_CTL_ADD,
        fd,
        &event
    );

    if (result == -1) {
        throw makeSystemError("epoll_ctl add");
    }
}

void TcpServer::removeClient(int client_fd) {
    const int result = ::epoll_ctl(
        epoll_fd_,
        EPOLL_CTL_DEL,
        client_fd,
        nullptr
    );

    if (result == -1 && errno != ENOENT) {
        std::cerr
            << "epoll_ctl delete failed for fd "
            << client_fd
            << ": "
            << std::strerror(errno)
            << std::endl;
    }

    ::close(client_fd);

    std::cout << "Client disconnected from fd "
              << client_fd
              << std::endl;
}

}  // namespace redis::net