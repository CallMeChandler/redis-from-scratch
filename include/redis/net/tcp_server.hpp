#pragma once

#include "redis/command/command_handler.hpp"
#include "redis/config/server_config.hpp"
#include "redis/net/connection.hpp"
#include "redis/persistence/aof.hpp"
#include "redis/persistence/rdb.hpp"
#include "redis/protocol/resp_parser.hpp"
#include "redis/storage/database.hpp"
#include "redis/pubsub/pubsub_manager.hpp"
#include "redis/replication/replication_manager.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>

namespace redis::net {

class TcpServer {
public:
    explicit TcpServer(
        config::ServerConfig config
    );

    ~TcpServer();

    TcpServer(
        const TcpServer&
    ) = delete;

    TcpServer& operator=(
        const TcpServer&
    ) = delete;

    TcpServer(
        TcpServer&&
    ) = delete;

    TcpServer& operator=(
        TcpServer&&
    ) = delete;

    void start();

private:
    void createSocket();
    void configureSocket();
    void bindSocket();
    void listenForConnections();

    void createEpollInstance();
    void registerServerSocket();

    void setupSignalHandling();
    void handleSignalEvent();

    void runEventLoop();

    void handleNewConnection();

    void handleClientEvent(
        int client_fd,
        std::uint32_t events
    );

    bool readFromClient(
        Connection& connection
    );

    bool writeToClient(
        Connection& connection
    );

    bool processInput(
        Connection& connection
    );

    void updateClientEvents(
        int client_fd,
        std::uint32_t events
    );

    void addToEpoll(
        int fd,
        std::uint32_t events
    );

    void removeClient(
        int client_fd
    );

    void performGracefulShutdown();

    bool handlePubSubCommand(
        Connection& connection,
        const std::vector<std::string>& arguments,
        const std::string& command
    );

    bool handleReplicationCommand(
        Connection& connection,
        const std::vector<std::string>& arguments,
        const std::string& command
    );

    void propagateReplicationWrite(
        const std::vector<std::string>& arguments
    );

    void connectToPrimary();

    void handlePrimaryEvent(
        std::uint32_t events
    );

    bool processReplicationInput();

    [[nodiscard]]
    bool isReplicaWriteForbidden(
        const std::string& command
    ) const;

    bool handleInfoCommand(
        Connection& connection,
        const std::vector<std::string>& arguments,
        const std::string& command
    );

    [[nodiscard]]
    std::string buildInfoPayload(
        const std::string& requested_section
    );

    config::ServerConfig config_;

    int server_fd_;
    int epoll_fd_;
    int signal_fd_;

    bool running_;

    std::unordered_map<
        int,
        std::unique_ptr<Connection>
    > connections_;

    protocol::RespParser resp_parser_;

    storage::Database database_;

    persistence::RdbSnapshot snapshot_;

    persistence::AppendOnlyFile aof_;

    pubsub::PubSubManager pubsub_manager_;

    command::CommandHandler command_handler_;

    replication::ReplicationManager replication_manager_;

    int primary_fd_;

    std::string replication_input_buffer_;

    command::TransactionState replication_transaction_;

    std::chrono::steady_clock::time_point
    started_at_;

    std::uint64_t total_connections_received_ = 0;

    std::uint64_t rejected_connections_ = 0;

    std::uint64_t total_commands_processed_ = 0;

    std::uint64_t total_net_input_bytes_ = 0;

    std::uint64_t total_net_output_bytes_ = 0;
};

}  // namespace redis::net