#pragma once

#include <string>
#include <string_view>
#include <atomic>
#include <thread>
#include <stop_token>

// Абстрактный базовый класс для всех сетевых endpoint-ов.
// Каждый endpoint работает в собственном std::jthread.
// Кооперативная отмена — через std::stop_token.
//
// ВАЖНО: Производный класс ОБЯЗАН первым делом в своём деструкторе
//        вызвать stop(), чтобы поток завершился до уничтожения
//        членов производного класса.
class Endpoint {
public:
    explicit Endpoint(std::string name) : name_(std::move(name)) {}
    virtual ~Endpoint() { stop(); }

    Endpoint(const Endpoint&)            = delete;
    Endpoint& operator=(const Endpoint&) = delete;

    void start() {
        running_.store(true, std::memory_order_relaxed);
        thread_ = std::jthread([this](std::stop_token st) {
            run(st);
            running_.store(false, std::memory_order_relaxed);
        });
    }

    // Запросить остановку и дождаться завершения потока.
    void stop() {
        if (thread_.joinable()) {
            thread_.request_stop();
            thread_.join();
        }
    }

    [[nodiscard]] std::string_view name()    const noexcept { return name_; }
    [[nodiscard]] bool             running() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }

protected:
    // Реализовать логику endpoint-а.
    // Должен возвращать управление при st.stop_requested() == true.
    virtual void run(std::stop_token st) = 0;

    std::string name_;

private:
    std::atomic<bool> running_{false};
    std::jthread      thread_;
};
