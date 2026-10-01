#pragma once

#include "redis/command/command_handler.hpp"
#include "redis/net/connection.hpp"
#include "redis/protocol/resp_parser.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace redis::net {

class TcpServer {
public:
    explicit TcpServer(std::uint16_t port);

    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    void start();

private:
    void createSocket();
    void configureSocket();
    void bindSocket();
    void listenForConnections();

    void createEpollInstance();
    void registerServerSocket();
    void runEventLoop();

    void handleNewConnection();

    void handleClientEvent(
        int client_fd,
        std::uint32_t events
    );

    bool readFromClient(Connection& connection);
    bool writeToClient(Connection& connection);

    bool processInput(Connection& connection);

    void updateClientEvents(
        int client_fd,
        std::uint32_t events
    );

    void addToEpoll(
        int fd,
        std::uint32_t events
    );

    void removeClient(int client_fd);

    std::uint16_t port_;
    int server_fd_;
    int epoll_fd_;

    std::unordered_map<
        int,
        std::unique_ptr<Connection>
    > connections_;

    protocol::RespParser resp_parser_;
    command::CommandHandler command_handler_;
};

}  // namespace redis::net