#pragma once

#include "codec/Message.hpp"

#include <cstdint>
#include <queue>
#include <span>
#include <string>

namespace avr {

// Streaming parser for AVR Standard (*HEXDATA;) and AVR MLAT (@TSHEX;).
// Fills codec::AdsMessage::frame[] directly — no intermediate allocations.
class AvrParser {
public:
    void feed(std::span<const uint8_t> data);

    [[nodiscard]] bool hasMessage() const noexcept { return !ready_.empty(); }

    codec::AdsMessage pop();
    void reset();

private:
    std::string                   lineBuf_;
    std::queue<codec::AdsMessage> ready_;

    static constexpr size_t kMaxLine = 512;

    void tryParseLine(std::string_view line);

    // Decode hex string into dst[0..maxLen-1].
    // Returns bytes written, 0 on error (odd length, unknown chars, overflow).
    static uint8_t parseHexInto(std::string_view hex,
                                  uint8_t* dst,
                                  uint8_t maxLen) noexcept;

    static codec::FrameType classifyFrame(uint8_t n) noexcept;
    static uint8_t          beastTypeFor (uint8_t n) noexcept;
};

} // namespace avr
