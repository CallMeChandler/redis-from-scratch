#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace redis::pubsub {

struct PublishResult {
    std::vector<int> subscriber_fds;
    std::string payload;
};

class PubSubManager {
public:
    [[nodiscard]]
    std::vector<std::string> subscribe(
        int client_fd,
        const std::vector<std::string>& channels
    );

    [[nodiscard]]
    std::vector<std::string> unsubscribe(
        int client_fd,
        const std::vector<std::string>& channels
    );

    [[nodiscard]]
    PublishResult publish(
        const std::string& channel,
        const std::string& message
    ) const;

    [[nodiscard]]
    bool isSubscribed(
        int client_fd
    ) const;

    void removeClient(
        int client_fd
    );

private:
    [[nodiscard]]
    std::size_t subscriptionCount(
        int client_fd
    ) const;

    [[nodiscard]]
    static std::string encodeSubscribe(
        const std::string& channel,
        std::size_t count
    );

    [[nodiscard]]
    static std::string encodeUnsubscribe(
        const std::string& channel,
        std::size_t count
    );

    [[nodiscard]]
    static std::string encodeEmptyUnsubscribe();

    [[nodiscard]]
    static std::string encodeMessage(
        const std::string& channel,
        const std::string& message
    );

    std::unordered_map<
        std::string,
        std::unordered_set<int>
    > channels_;

    std::unordered_map<
        int,
        std::unordered_set<std::string>
    > subscriptions_;
};

}  // namespace redis::pubsub