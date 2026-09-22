#include "redis/net/tcp_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace redis::net {

namespace {

constexpr int kInvalidFileDescriptor = -1;
constexpr int kListenBacklog = 128;
constexpr std::size_t kBufferSize = 4096;

std::runtime_error makeSystemError(const std::string& operation) {
    return std::runtime_error(
        operation+"failed"+std::strerror(errno)
    );
}

}

TcpServer::TcpServer(std::uint16_t port)
    : port_(port),
    server_fd_(kInvalidFileDescriptor){        
}

TcpServer::~TcpServer() {
    if (server_fd_!=kInvalidFileDescriptor){
        ::close(server_fd_);
    }
}

void TcpServer::start(){
    createSocket();
    configureSocket();
    bindSocket();
    listenForConnections();

    std::cout <<"Redis server listening on port "
        <<port_
        << '\n';

    acceptClient();
}

void TcpServer::createSocket(){
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd_ == kInvalidFileDescriptor){
        throw makeSystemError("socket");
    }
}

void TcpServer::configureSocket(){
    constexpr int enable = 1;

    const int result = ::setsockopt(
        server_fd_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &enable,
        sizeof(enable)
    );

    if (result == -1){
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

    if (result == -1){
        throw makeSystemError("bind");
    }
}

void TcpServer::listenForConnections() {
    const int result = ::listen(
        server_fd_,
        kListenBacklog
    );

    if (result == -1){
        throw makeSystemError("listen");
    }
}

void TcpServer::acceptClient() {
    sockaddr_in client_address{};
    socklen_t client_address_length = sizeof(client_address);

    const int client_fd = ::accept(
        server_fd_,
        reinterpret_cast<sockaddr*>(&client_address),
        &client_address_length
    );

    if (client_fd==kInvalidFileDescriptor){
        throw makeSystemError("accept");
    }

    char client_ip[INET_ADDRSTRLEN]{};

    const char* conversion_result = ::inet_ntop(
        AF_INET,
        &client_address.sin_addr,
        client_ip,
        sizeof(client_ip)
    );

    if (conversion_result==nullptr){
        ::close(client_fd);
        throw makeSystemError("inet_ntop");
    }
    
    std::cout << "Client connected from "
              << client_ip
              << ':'
              << ntohs(client_address.sin_port)
              << '\n';

    char buffer[kBufferSize]{};

    const ssize_t bytes_read = ::recv(
        client_fd,
        buffer,
        sizeof(buffer),
        0
    );

    if (bytes_read==-1){
        ::close(client_fd);
        throw makeSystemError("recv");
    }

    if (bytes_read==0){
        std::cout<<"Client disconnected without sending data\n";
        ::close(client_fd);
        return;
    }

    std::cout<<"Recieved "
            <<bytes_read
            <<" bytes\n";

    std::size_t total_bytes_sent = 0;

    while (total_bytes_sent < static_cast<std::size_t>(bytes_read)){
        const ssize_t bytes_sent = ::send(
            client_fd,
            buffer+total_bytes_sent,
            static_cast<std::size_t>(bytes_read)- total_bytes_sent,
            0
        );

        if (bytes_sent == -1){
            ::close(client_fd);
            throw makeSystemError("send");
        }

        total_bytes_sent += static_cast<std::size_t>(bytes_sent);
    }

    ::close(client_fd);

    std::cout<<"Client connection closed\n";
}

}