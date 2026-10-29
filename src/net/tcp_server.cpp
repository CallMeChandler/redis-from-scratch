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
#include <variant>
#include <utility>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <cctype>
#include <sstream>

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
    : config_(std::move(config)),
      server_fd_(kInvalidFileDescriptor),
      epoll_fd_(kInvalidFileDescriptor),
      signal_fd_(kInvalidFileDescriptor),
      running_(false),
      connections_(),
      resp_parser_(),
      database_(),
      snapshot_(config_.rdb_path),
      aof_(config_.aof_path),
      pubsub_manager_(),
      command_handler_(
          database_,
          snapshot_,
          aof_
      ),
      replication_manager_(),
      primary_fd_(kInvalidFileDescriptor),
      replication_input_buffer_(),
      replication_transaction_() {
    database_.setMaxMemory(
        config_.max_memory_bytes
    );

    if (aof_.existsAndNotEmpty()) {
        std::vector<protocol::RespValue> aof_commands;

        if (!aof_.load(aof_commands)) {
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
        if (!snapshot_.load(database_)) {
            throw std::runtime_error(
                "failed to load RDB snapshot"
            );
        }

        std::cout
            << "Recovered database from RDB"
            << std::endl;
    }

    if (!config_.isReplica()) {
        command_handler_.setWriteObserver(
            [this](
                const std::vector<std::string>& arguments
            ) {
                propagateReplicationWrite(
                    arguments
                );
            }
        );
    }
}

TcpServer::~TcpServer() {
    connections_.clear();

    if (
        primary_fd_ !=
        kInvalidFileDescriptor
    ) {
        ::close(primary_fd_);
    }

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
    started_at_= std::chrono::steady_clock::now();
    createSocket();
    configureSocket();
    bindSocket();
    listenForConnections();

    createEpollInstance();
    registerServerSocket();
    setupSignalHandling();

    if (config_.isReplica()) {
        connectToPrimary();
    }

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

            if (
                file_descriptor ==
                primary_fd_
            ) {
                handlePrimaryEvent(
                    event_flags
                );
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

            ++rejected_connections_;

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

        ++total_connections_received_;

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

            total_net_input_bytes_+=
                static_cast<std::uint64_t>(
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

        std::vector<std::string> arguments;
        std::string command;

        const bool decoded =
            command::CommandHandler::decodeCommand(
                result.value,
                arguments,
                command
            );

        if (decoded) {
            ++total_commands_processed_;
            if (
                handleReplicationCommand(
                    connection,
                    arguments,
                    command
                )
            ) {
                continue;
            }

            if (
                handlePubSubCommand(
                    connection,
                    arguments,
                    command
                )
            ) {
                continue;
            }

            if (
                isReplicaWriteForbidden(
                    command
                )
            ) {
                connection.appendOutput(
                    "-READONLY You can't write against a read only replica.\r\n"
                );

                continue;
            }
            if (
                command::CommandHandler::decodeCommand(
                    result.value,
                    arguments,
                    command
                )
            ) {
                ++total_commands_processed_;

                if (
                    handleReplicationCommand(
                        connection,
                        arguments,
                        command
                    )
                ) {
                    continue;
                }

                if (
                    isReplicaWriteForbidden(
                        command
                    )
                ) {
                    connection.appendOutput(
                        "-READONLY You can't write against a read only replica.\r\n"
                    );

                    continue;
                }

                if (
                    handlePubSubCommand(
                        connection,
                        arguments,
                        command
                    )
                ) {
                    continue;
                }

                if (
                    handleInfoCommand(
                        connection,
                        arguments,
                        command
                    )
                ) {
                    continue;
                }
            }
        }

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

bool TcpServer::handlePubSubCommand(

    Connection& connection,

    const std::vector<std::string>& arguments,

    const std::string& command

) {

    const int client_fd =

        connection.fd();

    if (

        command == "SUBSCRIBE" ||

        command == "UNSUBSCRIBE" ||

        command == "PUBLISH"

    ) {

        if (

            connection

                .transaction()

                .active()

        ) {

            connection.appendOutput(

                "-ERR Pub/Sub commands are not supported inside MULTI\r\n"

            );

            return true;

        }

    }

    if (command == "SUBSCRIBE") {

        if (arguments.size() < 2) {

            connection.appendOutput(

                "-ERR wrong number of arguments for 'subscribe' command\r\n"

            );

            return true;

        }

        const std::vector<std::string> channels(

            arguments.begin() + 1,

            arguments.end()

        );

        const std::vector<std::string> responses =

            pubsub_manager_.subscribe(

                client_fd,

                channels

            );

        for (

            const std::string& response :

            responses

        ) {

            connection.appendOutput(

                response

            );

        }

        return true;

    }

    if (command == "UNSUBSCRIBE") {

        const std::vector<std::string> channels(

            arguments.begin() + 1,

            arguments.end()

        );

        const std::vector<std::string> responses =

            pubsub_manager_.unsubscribe(

                client_fd,

                channels

            );

        for (

            const std::string& response :

            responses

        ) {

            connection.appendOutput(

                response

            );

        }

        return true;

    }

    if (

        pubsub_manager_.isSubscribed(

            client_fd

        ) &&

        command != "PING"

    ) {

        connection.appendOutput(

            "-ERR only SUBSCRIBE, UNSUBSCRIBE and PING are allowed while subscribed\r\n"

        );

        return true;

    }

    if (command == "PUBLISH") {

        if (arguments.size() != 3) {

            connection.appendOutput(

                "-ERR wrong number of arguments for 'publish' command\r\n"

            );

            return true;

        }

        const pubsub::PublishResult result =

            pubsub_manager_.publish(

                arguments[1],

                arguments[2]

            );

        std::vector<int> dead_clients;

        for (

            const int subscriber_fd :

            result.subscriber_fds

        ) {

            const auto iterator =

                connections_.find(

                    subscriber_fd

                );

            if (

                iterator ==

                connections_.end()

            ) {

                dead_clients.push_back(

                    subscriber_fd

                );

                continue;

            }

            Connection& subscriber =

                *iterator->second;

            subscriber.appendOutput(

                result.payload

            );

            if (

                !writeToClient(

                    subscriber

                )

            ) {

                dead_clients.push_back(

                    subscriber_fd

                );

            }

        }

        for (

            const int dead_fd :

            dead_clients

        ) {

            removeClient(

                dead_fd

            );

        }

        connection.appendOutput(

            ":" +

            std::to_string(

                result.subscriber_fds.size()

            ) +

            "\r\n"

        );

        return true;

    }

    return false;

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

    pubsub_manager_.removeClient(
        client_fd
    );

    replication_manager_.removeReplica(
        client_fd
    );

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

    connections_.clear();

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

bool TcpServer::handleReplicationCommand(
    Connection& connection,
    const std::vector<std::string>& arguments,
    const std::string& command
) {
    if (command != "SYNC") {
        return false;
    }

    if (arguments.size() != 1) {
        connection.appendOutput(
            "-ERR wrong number of arguments for 'sync' command\r\n"
        );

        return true;
    }

    if (config_.isReplica()) {
        connection.appendOutput(
            "-ERR replicas cannot serve SYNC\r\n"
        );

        return true;
    }

    replication_manager_.addReplica(
        connection.fd()
    );

    connection.appendOutput(
        replication::
            ReplicationManager::
            buildFullSync(
                database_
            )
    );

    std::cout
        << "Replica registered on fd "
        << connection.fd()
        << std::endl;

    return true;
}

void TcpServer::propagateReplicationWrite(
    const std::vector<std::string>& arguments
) {
    if (
        replication_manager_
            .replicas()
            .empty()
    ) {
        return;
    }

    const std::string payload =
        replication::
            ReplicationManager::
            encodeCommand(
                arguments
            );

    std::vector<int> dead_replicas;

    for (
        const int replica_fd :
        replication_manager_
            .replicas()
    ) {
        const auto iterator =
            connections_.find(
                replica_fd
            );

        if (
            iterator ==
            connections_.end()
        ) {
            dead_replicas.push_back(
                replica_fd
            );

            continue;
        }

        Connection& replica =
            *iterator->second;

        replica.appendOutput(
            payload
        );

        if (
            !writeToClient(
                replica
            )
        ) {
            dead_replicas.push_back(
                replica_fd
            );
        }
    }

    for (
        const int replica_fd :
        dead_replicas
    ) {
        removeClient(
            replica_fd
        );
    }
}

void TcpServer::handlePrimaryEvent(
    std::uint32_t events
) {
    if (
        (events & EPOLLERR) != 0U ||
        (events & EPOLLHUP) != 0U ||
        (events & EPOLLRDHUP) != 0U
    ) {
        std::cerr
            << "Primary connection lost"
            << std::endl;

        ::close(
            primary_fd_
        );

        primary_fd_ =
            kInvalidFileDescriptor;

        return;
    }

    if (
        (events & EPOLLIN) == 0U
    ) {
        return;
    }

    char buffer[
        kReadBufferSize
    ];

    while (true) {
        const ssize_t bytes =
            ::recv(
                primary_fd_,
                buffer,
                sizeof(buffer),
                0
            );

        if (bytes > 0) {
            replication_input_buffer_.append(
                buffer,
                static_cast<std::size_t>(
                    bytes
                )
            );

            total_net_input_bytes_ +=
                static_cast<std::uint64_t>(
                    bytes
                );

            continue;
        }

        if (bytes == 0) {
            std::cerr
                << "Primary disconnected"
                << std::endl;

            ::close(
                primary_fd_
            );

            primary_fd_ =
                kInvalidFileDescriptor;

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

        throw makeSystemError(
            "recv from primary"
        );
    }

    if (!processReplicationInput()) {
        throw std::runtime_error(
            "invalid replication stream"
        );
    }
}

bool TcpServer::processReplicationInput() {
    while (
        !replication_input_buffer_
            .empty()
    ) {
        const protocol::ParseResult result =
            resp_parser_.parse(
                replication_input_buffer_
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
            return false;
        }

        replication_input_buffer_.erase(
            0,
            result.consumed
        );

        const protocol::SimpleString* simple =
            std::get_if<
                protocol::SimpleString
            >(
                &result.value.value
            );

        if (simple != nullptr) {
            if (
                simple->value ==
                "FULLRESYNC"
            ) {
                database_.clear();

                if (!aof_.reset()) {
                    throw std::runtime_error(
                        "failed to reset replica AOF"
                    );
                }

                replication_transaction_
                    .discard();

                std::cout
                    << "Full replication sync started"
                    << std::endl;

                continue;
            }

            if (
                simple->value ==
                "FULLSYNCEND"
            ) {
                std::cout
                    << "Full replication sync complete"
                    << std::endl;

                continue;
            }
        }

        const std::string response =
            command_handler_.execute(
                result.value,
                replication_transaction_
            );

        if (
            !response.empty() &&
            response.front() == '-'
        ) {
            std::cerr
                << "Replication command failed: "
                << response;

            return false;
        }
    }

    return true;
}

bool TcpServer::isReplicaWriteForbidden(
    const std::string& command
) const {
    if (!config_.isReplica()) {
        return false;
    }

    return
        replication::
            ReplicationManager::
            isWriteCommand(
                command
            ) ||
        command == "MULTI" ||
        command == "EXEC" ||
        command == "DISCARD";
}

void TcpServer::connectToPrimary() {

    primary_fd_ =

        ::socket(

            AF_INET,

            SOCK_STREAM |

                SOCK_CLOEXEC,

            0

        );

    if (

        primary_fd_ ==

        kInvalidFileDescriptor

    ) {

        throw makeSystemError(

            "replica socket"

        );

    }

    sockaddr_in address{};

    address.sin_family =

        AF_INET;

    address.sin_port =

        htons(

            config_.replica_of_port

        );

    if (

        ::inet_pton(

            AF_INET,

            config_

                .replica_of_host

                .c_str(),

            &address.sin_addr

        ) != 1

    ) {

        throw std::runtime_error(

            "invalid replicaof host"

        );

    }

    if (

        ::connect(

            primary_fd_,

            reinterpret_cast<

                const sockaddr*

            >(

                &address

            ),

            sizeof(address)

        ) == -1

    ) {

        throw makeSystemError(

            "connect to primary"

        );

    }

    const std::string sync_command =

        replication::

            ReplicationManager::

            encodeCommand({

                "SYNC"

            });

    std::size_t sent_total = 0;

    while (
        sent_total <
        sync_command.size()
    ) {
        const ssize_t sent =
            ::send(
                primary_fd_,
                sync_command.data() +
                    sent_total,
                sync_command.size() -
                    sent_total,
                MSG_NOSIGNAL
            );

        if (sent > 0) {
            sent_total +=
                static_cast<std::size_t>(
                    sent
                );

            continue;
        }

        if (
            sent == -1 &&
            errno == EINTR
        ) {
            continue;
        }

        throw std::runtime_error(
            "failed to send SYNC to primary"
        );
    }

    const int flags =

        ::fcntl(

            primary_fd_,

            F_GETFL,

            0

        );

    if (flags == -1) {

        throw makeSystemError(

            "fcntl get primary flags"

        );

    }

    if (

        ::fcntl(

            primary_fd_,

            F_SETFL,

            flags | O_NONBLOCK

        ) == -1

    ) {

        throw makeSystemError(

            "fcntl set primary nonblocking"

        );

    }

    addToEpoll(

        primary_fd_,

        EPOLLIN |

            EPOLLRDHUP

    );

    std::cout

        << "Connected to primary "

        << config_.replica_of_host

        << ':'

        << config_.replica_of_port

        << std::endl;

}

bool TcpServer::handleInfoCommand(
    Connection& connection,
    const std::vector<std::string>& arguments,
    const std::string& command
) {
    if (command != "INFO") {
        return false;
    }

    if (arguments.size() > 2) {
        connection.appendOutput(
            "-ERR wrong number of arguments for 'info' command\r\n"
        );

        return true;
    }

    std::string section =
        arguments.size() == 2
            ? arguments[1]
            : "all";

    std::transform(
        section.begin(),
        section.end(),
        section.begin(),
        [](
            unsigned char character
        ) {
            return static_cast<char>(
                std::tolower(
                    character
                )
            );
        }
    );

    const std::string payload =
        buildInfoPayload(
            section
        );

    if (
        payload.empty() &&
        section != "all"
    ) {
        connection.appendOutput(
            "-ERR unknown INFO section\r\n"
        );

        return true;
    }

    connection.appendOutput(
        "$" +
        std::to_string(
            payload.size()
        ) +
        "\r\n" +
        payload +
        "\r\n"
    );

    return true;
}

std::string TcpServer::buildInfoPayload(
    const std::string& requested_section
) {
    const bool all =
        requested_section == "all";

    std::ostringstream output;

    if (
        all ||
        requested_section == "server"
    ) {
        const auto uptime =
            std::chrono::duration_cast<
                std::chrono::seconds
            >(
                std::chrono::steady_clock::now() -
                started_at_
            ).count();

        output
            << "# Server\r\n"
            << "redis_version:redis-from-scratch\r\n"
            << "uptime_in_seconds:"
            << uptime
            << "\r\n"
            << "tcp_port:"
            << config_.port
            << "\r\n"
            << "\r\n";
    }

    if (
        all ||
        requested_section == "clients"
    ) {
        const std::size_t replica_connections =
            replication_manager_
                .replicas()
                .size();

        std::size_t connected_clients =
            connections_.size();

        if (
            connected_clients >=
            replica_connections
        ) {
            connected_clients -=
                replica_connections;
        }

        output
            << "# Clients\r\n"
            << "connected_clients:"
            << connected_clients
            << "\r\n"
            << "total_connections_received:"
            << total_connections_received_
            << "\r\n"
            << "rejected_connections:"
            << rejected_connections_
            << "\r\n"
            << "\r\n";
    }

    if (
        all ||
        requested_section == "memory"
    ) {
        output
            << "# Memory\r\n"
            << "used_memory:"
            << database_
                   .estimatedMemoryUsage()
            << "\r\n"
            << "maxmemory:"
            << database_
                   .maxMemory()
            << "\r\n"
            << "\r\n";
    }

    if (
        all ||
        requested_section == "stats"
    ) {
        output
            << "# Stats\r\n"
            << "total_commands_processed:"
            << total_commands_processed_
            << "\r\n"
            << "total_net_input_bytes:"
            << total_net_input_bytes_
            << "\r\n"
            << "total_net_output_bytes:"
            << total_net_output_bytes_
            << "\r\n"
            << "expired_keys:"
            << database_
                   .expiredKeysCount()
            << "\r\n"
            << "evicted_keys:"
            << database_
                   .evictedKeysCount()
            << "\r\n"
            << "\r\n";
    }

    if (
        all ||
        requested_section ==
            "replication"
    ) {
        output
            << "# Replication\r\n";

        if (config_.isReplica()) {
            output
                << "role:replica\r\n"
                << "primary_host:"
                << config_.replica_of_host
                << "\r\n"
                << "primary_port:"
                << config_.replica_of_port
                << "\r\n"
                << "primary_link_status:"
                << (
                       primary_fd_ !=
                               kInvalidFileDescriptor
                           ? "up"
                           : "down"
                   )
                << "\r\n";
        } else {
            output
                << "role:primary\r\n"
                << "connected_replicas:"
                << replication_manager_
                       .replicas()
                       .size()
                << "\r\n";
        }

        output << "\r\n";
    }

    if (
        all ||
        requested_section ==
            "keyspace"
    ) {
        output
            << "# Keyspace\r\n"
            << "db0:keys="
            << database_.size()
            << ",expires="
            << database_
                   .expirationCount()
            << "\r\n"
            << "\r\n";
    }

    return output.str();
}

}  // namespace redis::net
