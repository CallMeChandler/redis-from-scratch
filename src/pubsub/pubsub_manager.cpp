#include "redis/pubsub/pubsub_manager.hpp"

#include <string>
#include <vector>

namespace redis::pubsub {

namespace {

std::string encodeBulkString(
    const std::string& value
) {
    return "$" +
           std::to_string(value.size()) +
           "\r\n" +
           value +
           "\r\n";
}

std::string encodeInteger(
    std::size_t value
) {
    return ":" +
           std::to_string(value) +
           "\r\n";
}

}  // namespace

std::vector<std::string>
PubSubManager::subscribe(
    int client_fd,
    const std::vector<std::string>& channels
) {
    std::vector<std::string> responses;

    auto& client_channels =
        subscriptions_[client_fd];

    for (
        const std::string& channel :
        channels
    ) {
        client_channels.insert(
            channel
        );

        channels_[channel].insert(
            client_fd
        );

        responses.push_back(
            encodeSubscribe(
                channel,
                client_channels.size()
            )
        );
    }

    return responses;
}

std::vector<std::string>
PubSubManager::unsubscribe(
    int client_fd,
    const std::vector<std::string>& requested_channels
) {
    std::vector<std::string> channels_to_remove;

    const auto client_iterator =
        subscriptions_.find(
            client_fd
        );

    if (requested_channels.empty()) {
        if (
            client_iterator ==
            subscriptions_.end()
        ) {
            return {
                encodeEmptyUnsubscribe()
            };
        }

        channels_to_remove.assign(
            client_iterator->second.begin(),
            client_iterator->second.end()
        );
    } else {
        channels_to_remove =
            requested_channels;
    }

    std::vector<std::string> responses;

    for (
        const std::string& channel :
        channels_to_remove
    ) {
        auto current_client =
            subscriptions_.find(
                client_fd
            );

        if (
            current_client !=
            subscriptions_.end()
        ) {
            current_client->second.erase(
                channel
            );

            if (
                current_client->second.empty()
            ) {
                subscriptions_.erase(
                    current_client
                );
            }
        }

        auto channel_iterator =
            channels_.find(
                channel
            );

        if (
            channel_iterator !=
            channels_.end()
        ) {
            channel_iterator->second.erase(
                client_fd
            );

            if (
                channel_iterator->second.empty()
            ) {
                channels_.erase(
                    channel_iterator
                );
            }
        }

        responses.push_back(
            encodeUnsubscribe(
                channel,
                subscriptionCount(
                    client_fd
                )
            )
        );
    }

    return responses;
}

PublishResult PubSubManager::publish(
    const std::string& channel,
    const std::string& message
) const {
    PublishResult result;

    const auto iterator =
        channels_.find(
            channel
        );

    if (
        iterator ==
        channels_.end()
    ) {
        return result;
    }

    result.subscriber_fds.assign(
        iterator->second.begin(),
        iterator->second.end()
    );

    result.payload =
        encodeMessage(
            channel,
            message
        );

    return result;
}

bool PubSubManager::isSubscribed(
    int client_fd
) const {
    const auto iterator =
        subscriptions_.find(
            client_fd
        );

    return
        iterator != subscriptions_.end() &&
        !iterator->second.empty();
}

void PubSubManager::removeClient(
    int client_fd
) {
    const auto iterator =
        subscriptions_.find(
            client_fd
        );

    if (
        iterator ==
        subscriptions_.end()
    ) {
        return;
    }

    const std::vector<std::string> channels(
        iterator->second.begin(),
        iterator->second.end()
    );

    for (
        const std::string& channel :
        channels
    ) {
        auto channel_iterator =
            channels_.find(
                channel
            );

        if (
            channel_iterator ==
            channels_.end()
        ) {
            continue;
        }

        channel_iterator->second.erase(
            client_fd
        );

        if (
            channel_iterator->second.empty()
        ) {
            channels_.erase(
                channel_iterator
            );
        }
    }

    subscriptions_.erase(
        client_fd
    );
}

std::size_t PubSubManager::subscriptionCount(
    int client_fd
) const {
    const auto iterator =
        subscriptions_.find(
            client_fd
        );

    if (
        iterator ==
        subscriptions_.end()
    ) {
        return 0;
    }

    return iterator->second.size();
}

std::string PubSubManager::encodeSubscribe(
    const std::string& channel,
    std::size_t count
) {
    return "*3\r\n" +
           encodeBulkString("subscribe") +
           encodeBulkString(channel) +
           encodeInteger(count);
}

std::string PubSubManager::encodeUnsubscribe(
    const std::string& channel,
    std::size_t count
) {
    return "*3\r\n" +
           encodeBulkString("unsubscribe") +
           encodeBulkString(channel) +
           encodeInteger(count);
}

std::string PubSubManager::encodeEmptyUnsubscribe() {
    return "*3\r\n" +
           encodeBulkString("unsubscribe") +
           "$-1\r\n" +
           encodeInteger(0);
}

std::string PubSubManager::encodeMessage(
    const std::string& channel,
    const std::string& message
) {
    return "*3\r\n" +
           encodeBulkString("message") +
           encodeBulkString(channel) +
           encodeBulkString(message);
}

}  // namespace redis::pubsub