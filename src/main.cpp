#include "redis/net/tcp_server.hpp"

#include <cstdint>
#include <exception>
#include <iostream>

int main(){
    constexpr std::uint16_t kRedisPort = 6379;

    try {
        redis::net::TcpServer server(kRedisPort);
        server.start();
    } catch (const std::exception& exception){
        std::cerr<<"Fatal error: "
            <<exception.what()
            <<'\n';

        return 1;
    }

    return 0;
    
}