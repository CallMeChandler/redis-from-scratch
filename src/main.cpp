#include "redis/config/server_config.hpp"
#include "redis/net/tcp_server.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <utility>

int main(
    int argc,
    char* argv[]
) {
    try {
        if (argc > 2) {
            std::cerr
                << "Usage: "
                << argv[0]
                << " [config-file]"
                << std::endl;

            return 1;
        }

        redis::config::ServerConfig config;

        if (argc == 2) {
            config =
                redis::config::ServerConfig::
                    loadFromFile(
                        argv[1]
                    );

            std::cout
                << "Loaded configuration from "
                << argv[1]
                << std::endl;
        }

        redis::net::TcpServer server(
            std::move(config)
        );

        server.start();

        return 0;
    } catch (
        const std::exception& exception
    ) {
        std::cerr
            << "Fatal error: "
            << exception.what()
            << std::endl;

        return 1;
    }
}