#pragma once

#include "codec/Message.hpp"

#include <cstdint>
#include <queue>
#include <span>
#include <vector>

namespace beast {

inline constexpr uint8_t ESC = 0x1A;

// Ожидаемый размер тела кадра (байты после байта типа, без экранирования).
// -1 для неизвестных типов.
constexpr int bodySize(uint8_t type) noexcept {
    switch (type) {
        case '1': return  9;   // 6 MLAT + 1 signal + 2 Mode-AC
        case '2': return 14;   // 6 MLAT + 1 signal + 7 short Mode-S
        case '3': return 21;   // 6 MLAT + 1 signal + 14 long Mode-S
        case '4': return 21;   // 6 MLAT + 1 unused + DIP/конфигурация
        case '5': return 21;   // extended
        default:  return -1;
    }
}

// Сырые байты keepalive-сообщения (ESC '1' + 9 нулевых байт).
inline std::vector<uint8_t> heartbeatBytes() {
    return { 0x1A, '1', 0,0,0,0,0,0,0,0,0 };
}

// Потоковый парсер Beast Binary.
// Возвращает декодированный codec::AdsMessage для каждого полного кадра.
class BeastParser {
public:
    void feed(std::span<const uint8_t> data);

    [[nodiscard]] bool hasMessage() const noexcept { return !ready_.empty(); }

    codec::AdsMessage pop();
    void reset();

private:
    void process();

    std::vector<uint8_t>          buf_;
    std::queue<codec::AdsMessage> ready_;

    static constexpr size_t kMaxBuf = 65536;
};

} // namespace beast
