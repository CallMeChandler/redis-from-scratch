#pragma once

#include <cstdint>
#include <string>

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
    void acceptClient();

    std::uint16_t port_;
    int server_fd_;
};

}