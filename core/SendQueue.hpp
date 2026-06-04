#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <stop_token>

#include "codec/Message.hpp"

// Потокобезопасная ограниченная очередь (много писателей — один читатель).
// При переполнении удаляется самое старое сообщение без блокировки издателя.
class SendQueue {
public:
    enum class PopResult { Ok, Timeout, Stopped };

    explicit SendQueue(size_t maxSize = 512) : maxSize_(maxSize) {}

    void push(std::shared_ptr<codec::AdsMessage> msg) {
        {
            std::lock_guard lock(mutex_);
            if (closed_) return;
            if (queue_.size() >= maxSize_) {
                queue_.pop_front();
                dropped_.fetch_add(1, std::memory_order_relaxed);
            }
            queue_.push_back(std::move(msg));
        }
        cv_.notify_one();
    }

    // C++20: wait_for с stop_token — пробуждается автоматически при st.stop_requested()
    PopResult pop(std::shared_ptr<codec::AdsMessage>& out,
                  std::chrono::milliseconds timeout,
                  std::stop_token st) {
        std::unique_lock lock(mutex_);
        const bool ready = cv_.wait_for(lock, st, timeout,
            [this]{ return !queue_.empty() || closed_; });

        if (ready && !queue_.empty()) {
            out = std::move(queue_.front());
            queue_.pop_front();
            return PopResult::Ok;
        }
        if (st.stop_requested() || closed_) return PopResult::Stopped;
        return PopResult::Timeout;
    }

    void close() {
        { std::lock_guard lock(mutex_); closed_ = true; }
        cv_.notify_all();
    }

    [[nodiscard]] uint64_t dropped() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    mutable std::mutex                             mutex_;
    std::condition_variable_any                    cv_;
    std::deque<std::shared_ptr<codec::AdsMessage>> queue_;
    const size_t                                   maxSize_;
    bool                                           closed_  = false;
    std::atomic<uint64_t>                          dropped_{0};
};
