#include "redis/net/tcp_server.hpp"

#include <arpa/inet.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

namespace redis::net {

namespace {

constexpr int kInvalidFileDescriptor = -1;

constexpr int kMaxEvents = 64;

constexpr std::size_t kReadBufferSize =
    4096;

std::runtime_error makeSystemError(
    const std::string& operation
) {
    return std::runtime_error(
        operation +
        " failed: " +
        std::strerror(errno)
    );
}

}  // namespace

TcpServer::TcpServer(
    config::ServerConfig config
)
    : config_(
          std::move(config)
      ),
      server_fd_(
          kInvalidFileDescriptor
      ),
      epoll_fd_(
          kInvalidFileDescriptor
      ),
      signal_fd_(
          kInvalidFileDescriptor
      ),
      running_(false),
      connections_(),
      resp_parser_(),
      database_(),
      snapshot_(
          config_.rdb_path
      ),
      aof_(
          config_.aof_path
      ),
      command_handler_(
          database_,
          snapshot_,
          aof_
      ) {
    if (
        aof_.existsAndNotEmpty()
    ) {
        std::vector<
            protocol::RespValue
        > aof_commands;

        if (
            !aof_.load(
                aof_commands
            )
        ) {
            throw std::runtime_error(
                "failed to load AOF"
            );
        }

        for (
            const protocol::RespValue& command :
            aof_commands
        ) {
            const std::string response =
                command_handler_.replay(
                    command
                );

            if (
                !response.empty() &&
                response.front() == '-'
            ) {
                throw std::runtime_error(
                    "failed while replaying AOF"
                );
            }
        }

        std::cout
            << "Recovered database from AOF"
            << std::endl;
    } else {
        if (
            !snapshot_.load(
                database_
            )
        ) {
            throw std::runtime_error(
                "failed to load RDB snapshot"
            );
        }

        std::cout
            << "Recovered database from RDB"
            << std::endl;
    }
}

TcpServer::~TcpServer() {
    connections_.clear();

    if (
        signal_fd_ !=
        kInvalidFileDescriptor
    ) {
        ::close(signal_fd_);
    }

    if (
        epoll_fd_ !=
        kInvalidFileDescriptor
    ) {
        ::close(epoll_fd_);
    }

    if (
        server_fd_ !=
        kInvalidFileDescriptor
    ) {
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

    setupSignalHandling();

    std::cout
        << "Redis server listening on "
        << config_.bind_address
        << ':'
        << config_.port
        << std::endl;

    runEventLoop();

    performGracefulShutdown();
}

void TcpServer::createSocket() {
    server_fd_ =
        ::socket(
            AF_INET,
            SOCK_STREAM |
                SOCK_NONBLOCK |
                SOCK_CLOEXEC,
            0
        );

    if (
        server_fd_ ==
        kInvalidFileDescriptor
    ) {
        throw makeSystemError(
            "socket"
        );
    }
}

void TcpServer::configureSocket() {
    constexpr int enable = 1;

    const int result =
        ::setsockopt(
            server_fd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &enable,
            sizeof(enable)
        );

    if (result == -1) {
        throw makeSystemError(
            "setsockopt"
        );
    }
}

void TcpServer::bindSocket() {
    sockaddr_in server_address{};

    server_address.sin_family =
        AF_INET;

    if (
        config_.bind_address ==
        "0.0.0.0"
    ) {
        server_address.sin_addr.s_addr =
            htonl(INADDR_ANY);
    } else {
        const int result =
            ::inet_pton(
                AF_INET,
                config_.bind_address.c_str(),
                &server_address.sin_addr
            );

        if (result != 1) {
            throw std::runtime_error(
                "invalid bind address: " +
                config_.bind_address
            );
        }
    }

    server_address.sin_port =
        htons(config_.port);

    const int result =
        ::bind(
            server_fd_,
            reinterpret_cast<
                const sockaddr*
            >(
                &server_address
            ),
            sizeof(server_address)
        );

    if (result == -1) {
        throw makeSystemError(
            "bind"
        );
    }
}

void TcpServer::listenForConnections() {
    const int result =
        ::listen(
            server_fd_,
            config_.listen_backlog
        );

    if (result == -1) {
        throw makeSystemError(
            "listen"
        );
    }
}

void TcpServer::createEpollInstance() {
    epoll_fd_ =
        ::epoll_create1(
            EPOLL_CLOEXEC
        );

    if (
        epoll_fd_ ==
        kInvalidFileDescriptor
    ) {
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

void TcpServer::setupSignalHandling() {
    /*
     * signalfd only receives signals that are blocked
     * from normal process delivery.
     */
    sigset_t signal_mask;

    if (
        ::sigemptyset(
            &signal_mask
        ) == -1
    ) {
        throw makeSystemError(
            "sigemptyset"
        );
    }

    if (
        ::sigaddset(
            &signal_mask,
            SIGINT
        ) == -1
    ) {
        throw makeSystemError(
            "sigaddset SIGINT"
        );
    }

    if (
        ::sigaddset(
            &signal_mask,
            SIGTERM
        ) == -1
    ) {
        throw makeSystemError(
            "sigaddset SIGTERM"
        );
    }

    /*
     * Prevent the default signal behavior from
     * immediately terminating the process.
     */
    if (
        ::sigprocmask(
            SIG_BLOCK,
            &signal_mask,
            nullptr
        ) == -1
    ) {
        throw makeSystemError(
            "sigprocmask"
        );
    }

    signal_fd_ =
        ::signalfd(
            -1,
            &signal_mask,
            SFD_NONBLOCK |
                SFD_CLOEXEC
        );

    if (
        signal_fd_ ==
        kInvalidFileDescriptor
    ) {
        throw makeSystemError(
            "signalfd"
        );
    }

    /*
     * Signals are now just another epoll event.
     */
    addToEpoll(
        signal_fd_,
        EPOLLIN
    );
}

void TcpServer::handleSignalEvent() {
    while (true) {
        signalfd_siginfo signal_info{};

        const ssize_t bytes_read =
            ::read(
                signal_fd_,
                &signal_info,
                sizeof(signal_info)
            );

        if (
            bytes_read ==
            static_cast<ssize_t>(
                sizeof(signal_info)
            )
        ) {
            if (
                signal_info.ssi_signo ==
                    SIGINT ||
                signal_info.ssi_signo ==
                    SIGTERM
            ) {
                std::cout
                    << "Shutdown signal received"
                    << std::endl;

                running_ = false;

                return;
            }

            continue;
        }

        if (
            bytes_read == -1 &&
            errno == EINTR
        ) {
            continue;
        }

        if (
            bytes_read == -1 &&
            (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            )
        ) {
            return;
        }

        if (bytes_read == 0) {
            return;
        }

        throw makeSystemError(
            "read signalfd"
        );
    }
}

void TcpServer::runEventLoop() {
    std::array<
        epoll_event,
        kMaxEvents
    > events{};

    running_ = true;

    while (running_) {
        const int ready_count =
            ::epoll_wait(
                epoll_fd_,
                events.data(),
                kMaxEvents,
                config_.epoll_timeout_ms
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
            const int file_descriptor =
                events[index].data.fd;

            const std::uint32_t event_flags =
                events[index].events;

            if (
                file_descriptor ==
                signal_fd_
            ) {
                handleSignalEvent();

                /*
                 * Do not process additional client work
                 * once shutdown has been requested.
                 */
                if (!running_) {
                    break;
                }

                continue;
            }

            if (
                file_descriptor ==
                server_fd_
            ) {
                handleNewConnection();
                continue;
            }

            handleClientEvent(
                file_descriptor,
                event_flags
            );
        }

        if (!running_) {
            break;
        }

        database_.activeExpireCycle(
            config_.expiration_batch_size
        );
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
                reinterpret_cast<
                    sockaddr*
                >(
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

        if (
            connections_.size() >=
            config_.max_clients
        ) {
            std::cerr
                << "Maximum client limit reached"
                << std::endl;

            ::close(client_fd);

            continue;
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

        if (
            conversion_result ==
            nullptr
        ) {
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
        connections_.find(
            client_fd
        );

    if (
        connection_iterator ==
        connections_.end()
    ) {
        return;
    }

    Connection& connection =
        *connection_iterator->second;

    if (
        (events & EPOLLERR) != 0U
    ) {
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

    if (
        (events & EPOLLIN) != 0U
    ) {
        const bool connection_alive =
            readFromClient(
                connection
            );

        if (!connection_alive) {
            removeClient(
                client_fd
            );

            return;
        }
    }

    if (
        (events & EPOLLOUT) != 0U
    ) {
        const bool connection_alive =
            writeToClient(
                connection
            );

        if (!connection_alive) {
            removeClient(
                client_fd
            );

            return;
        }
    }

    if (peer_closed) {
        if (
            connection.hasPendingOutput()
        ) {
            const bool connection_alive =
                writeToClient(
                    connection
                );

            if (!connection_alive) {
                removeClient(
                    client_fd
                );

                return;
            }
        }

        if (
            !connection.hasPendingOutput()
        ) {
            removeClient(
                client_fd
            );
        }
    }
}

bool TcpServer::readFromClient(
    Connection& connection
) {
    char buffer[
        kReadBufferSize
    ]{};

    bool peer_reached_eof =
        false;

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
                static_cast<
                    std::size_t
                >(
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
            peer_reached_eof =
                true;

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
        processInput(
            connection
        );

    if (!protocol_valid) {
        return false;
    }

    if (
        connection.hasPendingOutput()
    ) {
        const bool connection_alive =
            writeToClient(
                connection
            );

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
        !connection
             .inputBuffer()
             .empty()
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
                connection
                    .inputBuffer()
                    .size()
            );

            return true;
        }

        connection.consumeInput(
            result.consumed
        );

        const std::string response =
            command_handler_.execute(
                result.value,
                connection.transaction()
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
                static_cast<
                    std::size_t
                >(
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

    event.events =
        events;

    event.data.fd =
        client_fd;

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

    event.events =
        events;

    event.data.fd =
        fd;

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

void TcpServer::performGracefulShutdown() {
    std::cout
        << "Graceful shutdown started"
        << std::endl;

    /*
- Destroying Connection objects closes their
- sockets through RAII.
     */
    connections_.clear();

    /*
- Every AOF mutation is already flushed when
- AppendOnlyFile::append() completes.
     *
- We additionally write a final RDB snapshot so
- the latest state has a compact persistence
- representation.
     */
    if (config_.save_on_shutdown) {
        if (
            !snapshot_.save(
                database_
            )
        ) {
            std::cerr
                << "Warning: failed to save final RDB snapshot"
                << std::endl;
        } else {
            std::cout
                << "Final RDB snapshot saved"
                << std::endl;
        }
    }

    std::cout
        << "Graceful shutdown complete"
        << std::endl;
}

}  // namespace redis::net