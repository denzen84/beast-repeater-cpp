#pragma once

#include "codec/Message.hpp"
#include <cstdint>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <vector>

namespace avr {

// Потоковый парсер обоих текстовых форматов AVR.
//
// AVR Standard:  *HEXDATA;
// AVR MLAT:      @TSHEXDATA;   (TS = 12 hex-символов = 6-байтный MLAT timestamp)
//
// Разделитель кадров — символ ';'.
// Формат определяется per-кадр по первому символу ('*' или '@').
class AvrParser {
public:
    void feed(std::span<const uint8_t> data);

    [[nodiscard]] bool hasMessage() const noexcept { return !ready_.empty(); }

    codec::AdsMessage pop();
    void reset();

private:
    std::string                   lineBuf_;
    std::queue<codec::AdsMessage> ready_;

    static constexpr size_t kMaxLineBuf = 512;

    void tryParseLine(std::string_view line);

    static std::optional<codec::AdsMessage> parseAvrStd (std::string_view hexPart);
    static std::optional<codec::AdsMessage> parseAvrMlat(std::string_view hexPart);

    static std::optional<std::vector<uint8_t>> parseHex(std::string_view hex);
    static codec::FrameType classifyFrame(size_t bytes) noexcept;
    static uint8_t          beastTypeFor (size_t bytes) noexcept;
};

} // namespace avr
