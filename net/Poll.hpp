#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <stop_token>

#include "net/Socket.hpp"

namespace net {

enum class PollResult { Ready, Timeout, Stopped, Error };

// Ждать готовности fd к чтению.
// Опрашивает с интервалом ≤100 мс, чтобы реагировать на stop_token.
PollResult pollRead(int fd, std::chrono::milliseconds timeout,
                    std::stop_token st);

// Ждать готовности fd к записи.
PollResult pollWrite(int fd, std::chrono::milliseconds timeout,
                     std::stop_token st);

// Записать все len байт в неблокирующий fd.
// При EAGAIN ждёт готовности через pollWrite.
// Возвращает false при ошибке, закрытии соединения или stop.
bool writeAll(int fd, const uint8_t* data, size_t len, std::stop_token st);

// Разрешить имя хоста и установить неблокирующее TCP-соединение.
// Возвращает подключённый Socket или невалидный Socket при неудаче.
// DNS-резолвинг выполняется синхронно в потоке вызывающего
// (каждый коннектор изолирован в своём std::jthread).
Socket connectTcp(const std::string& host, uint16_t port,
                  std::chrono::milliseconds timeout,
                  std::stop_token st);

// Прерываемый сон: спит кусками по 100 мс, проверяя stop_token.
inline void sleepFor(std::chrono::milliseconds ms, std::stop_token st) {
    using namespace std::chrono;
    const auto deadline = steady_clock::now() + ms;
    while (!st.stop_requested()) {
        const auto left = duration_cast<milliseconds>(deadline - steady_clock::now());
        if (left <= milliseconds{0}) break;
        std::this_thread::sleep_for(std::min(left, milliseconds{100}));
    }
}

} // namespace net
