#include "core/MessageHub.hpp"
#include "compat/format.hpp"  // ← было: #include <format>

#include <algorithm>
#include <iostream>

std::shared_ptr<SendQueue> MessageHub::subscribe(std::string_view name) {
    auto q = std::make_shared<SendQueue>(512);
    {
        std::unique_lock lock(mutex_);
        cleanup();
        entries_.push_back({ std::string(name), q });
    }
    std::cerr << std::format("[Hub] + subscriber: {}\n", name);
    return q;
}

void MessageHub::unsubscribe(const std::shared_ptr<SendQueue>& q) {
    q->close();
    std::unique_lock lock(mutex_);
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&q](const Entry& e){ return e.queue.lock() == q; }),
        entries_.end());
}

void MessageHub::publish(std::shared_ptr<codec::AdsMessage> msg) {
    published_.fetch_add(1, std::memory_order_relaxed);
    std::shared_lock lock(mutex_);
    for (auto& entry : entries_)
        if (auto q = entry.queue.lock())
            q->push(msg);
}

void MessageHub::cleanup() {
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [](const Entry& e){ return e.queue.expired(); }),
        entries_.end());
}

MessageHub::Stats MessageHub::stats() const {
    std::shared_lock lock(mutex_);
    uint64_t dropped = 0;
    for (const auto& e : entries_)
        if (auto q = e.queue.lock()) dropped += q->dropped();
    return { published_.load(), dropped, entries_.size() };
}
