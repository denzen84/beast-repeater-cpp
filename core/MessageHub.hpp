#pragma once

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "codec/Message.hpp"
#include "core/SendQueue.hpp"

class MessageHub {
public:
    [[nodiscard]]
    std::shared_ptr<SendQueue> subscribe(std::string_view name);
    void unsubscribe(const std::shared_ptr<SendQueue>& q);
    void publish(std::shared_ptr<codec::AdsMessage> msg);

    struct Stats {
        uint64_t published{};
        uint64_t totalDropped{};
        size_t   subscribers{};
    };
    [[nodiscard]] Stats stats() const;

private:
    struct Entry {
        std::string              name;
        std::weak_ptr<SendQueue> queue;
    };

    mutable std::shared_mutex mutex_;
    std::vector<Entry>        entries_;
    std::atomic<uint64_t>     published_{0};

    void cleanup();
};
