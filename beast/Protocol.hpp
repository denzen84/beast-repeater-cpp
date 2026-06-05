#pragma once

#include "codec/Message.hpp"

#include <cstdint>
#include <queue>
#include <span>
#include <vector>

namespace beast {

inline constexpr uint8_t ESC = 0x1A;

// Expected unescaped body size in bytes (after the type byte). -1 = unknown.
constexpr int bodySize(uint8_t type) noexcept {
    switch (type) {
        case '1': return  9;   // 6 MLAT + 1 signal + 2 Mode-AC
        case '2': return 14;   // 6 MLAT + 1 signal + 7 short Mode-S
        case '3': return 21;   // 6 MLAT + 1 signal + 14 long Mode-S
        case '4': return 21;   // 6 MLAT + 1 unused + DIP/config data
        case '5': return 21;   // extended
        default:  return -1;
    }
}

// Raw bytes of a Beast keepalive heartbeat: ESC '1' + 9 zero bytes.
inline std::vector<uint8_t> heartbeatBytes() {
    return { 0x1A, '1', 0,0,0,0,0,0,0,0,0 };
}

// Streaming parser for Beast Binary protocol.
// Returns decoded codec::AdsMessage (with inline frame buffer) per frame.
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
