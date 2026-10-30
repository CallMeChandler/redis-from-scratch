#include "redis/protocol/resp_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

bool checkInput(
    const redis::protocol::RespParser& parser,
    const std::string& input
) {
    try {
        const redis::protocol::ParseResult result =
            parser.parse(input);

        if (
            result.status ==
            redis::protocol::ParseStatus::Complete
        ) {
            if (
                result.consumed == 0 ||
                result.consumed >
                    input.size()
            ) {
                std::cerr
                    << "Invalid consumed count"
                    << std::endl;

                return false;
            }
        }

        return true;
    } catch (
        const std::exception& exception
    ) {
        std::cerr
            << "Parser threw exception: "
            << exception.what()
            << std::endl;

        return false;
    } catch (...) {
        std::cerr
            << "Parser threw unknown exception"
            << std::endl;

        return false;
    }
}

}  // namespace

int main() {
    redis::protocol::RespParser parser;

    const std::vector<std::string> structured_cases{
        "*1\r\n$4\r\nPING\r\n",
        "*2\r\n$4\r\nECHO\r\n$5\r\nhello\r\n",
        "$0\r\n\r\n",
        "$-1\r\n",
        ":123\r\n",
        ":-999\r\n",
        "+OK\r\n",

        "$999999999\r\n",
        "*999999999\r\n",
        "$5\r\nabc",
        "$3\r\nabcXX",
        "*2\r\n$4\r\nPING\r\n",
        "garbage\r\n"
    };

    for (
        const std::string& input :
        structured_cases
    ) {
        if (
            !checkInput(
                parser,
                input
            )
        ) {
            return 1;
        }
    }

    std::mt19937_64 generator(
        0x5245444953ULL
    );

    std::uniform_int_distribution<
        std::size_t
    > length_distribution(
        0,
        4096
    );

    std::uniform_int_distribution<int>
        byte_distribution(
            0,
            255
        );

    constexpr std::size_t kIterations =
        100000;

    for (
        std::size_t iteration = 0;
        iteration < kIterations;
        ++iteration
    ) {
        const std::size_t length =
            length_distribution(
                generator
            );

        std::string input;

        input.resize(
            length
        );

        for (
            char& character :
            input
        ) {
            character =
                static_cast<char>(
                    byte_distribution(
                        generator
                    )
                );
        }

        if (
            !checkInput(
                parser,
                input
            )
        ) {
            std::cerr
                << "Fuzz failure at iteration "
                << iteration
                << std::endl;

            return 1;
        }
    }

    std::cout
        << "RESP fuzz test passed "
        << kIterations
        << " randomized inputs"
        << std::endl;

    return 0;
}