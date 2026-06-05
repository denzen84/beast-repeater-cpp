#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>  // plain cv — faster than cv_any
#include <deque>
#include <memory>
#include <mutex>
#include <stop_token>

#include "codec/Message.hpp"

// Thread-safe bounded queue (many producers, one consumer).
//
// Optimisations vs. previous version:
//  1. std::condition_variable + stop_callback   → avoids cv_any overhead
//  2. notify_one() only when queue was empty     → eliminates redundant wake-ups
//     (at 400 pkt/s × 4 outputs this saves ~1200 spurious wake-ups/s)
//  3. tryPop() for non-blocking batch drain      → fewer context switches
class SendQueue {
public:
    enum class PopResult { Ok, Timeout, Stopped };

    explicit SendQueue(size_t maxSize = 512) : maxSize_(maxSize) {}

    // Push: O(1). Notifies consumer only if it was idle (queue was empty).
    void push(std::shared_ptr<codec::AdsMessage> msg) {
        bool doNotify;
        {
            std::lock_guard lock(mutex_);
            if (closed_) return;
            doNotify = queue_.empty();          // notify iff consumer may be waiting
            if (queue_.size() >= maxSize_) {
                queue_.pop_front();
                dropped_.fetch_add(1, std::memory_order_relaxed);
            }
            queue_.push_back(std::move(msg));
        }
        if (doNotify) cv_.notify_one();
    }

    // Blocking pop with stop_token.
    // Uses stop_callback + plain condition_variable for lower overhead
    // than condition_variable_any (the C++20 stop-aware overload).
    PopResult pop(std::shared_ptr<codec::AdsMessage>& out,
                  std::chrono::milliseconds timeout,
                  std::stop_token st) {
        // Wakeup when stop is requested
        std::stop_callback onStop{ st, [this]{ cv_.notify_all(); } };

        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, timeout,
            [this, &st]{ return !queue_.empty() || closed_ || st.stop_requested(); });

        if (st.stop_requested() || (closed_ && queue_.empty()))
            return PopResult::Stopped;

        if (queue_.empty())
            return PopResult::Timeout;

        out = std::move(queue_.front());
        queue_.pop_front();
        return PopResult::Ok;
    }

    // Non-blocking pop: used to drain the queue after an initial blocking pop.
    // Avoids re-entering the condition variable — no syscall overhead.
    [[nodiscard]] bool tryPop(std::shared_ptr<codec::AdsMessage>& out) noexcept {
        std::lock_guard lock(mutex_);
        if (queue_.empty() || closed_) return false;
        out = std::move(queue_.front());
        queue_.pop_front();
        return true;
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
    std::condition_variable                        cv_;
    std::deque<std::shared_ptr<codec::AdsMessage>> queue_;
    const size_t                                   maxSize_;
    bool                                           closed_  = false;
    std::atomic<uint64_t>                          dropped_{0};
};
