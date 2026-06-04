#include "avr/Protocol.hpp"

namespace avr {

void AvrParser::feed(std::span<const uint8_t> data) {
    for (uint8_t b : data) {
        if (b == ';') {
            // Конец кадра — попытка парсинга накопленной строки
            if (!lineBuf_.empty()) {
                tryParseLine(lineBuf_);
                lineBuf_.clear();
            }
        } else if (b == '\r' || b == '\n') {
            // Переносы строк игнорируются (кадры оканчиваются на ';')
        } else {
            if (lineBuf_.size() < kMaxLineBuf)
                lineBuf_.push_back(static_cast<char>(b));
            else
                lineBuf_.clear(); // переполнение → сброс и ресинхронизация
        }
    }
}

codec::AdsMessage AvrParser::pop() {
    codec::AdsMessage m = std::move(ready_.front());
    ready_.pop();
    return m;
}

void AvrParser::reset() {
    lineBuf_.clear();
    while (!ready_.empty()) ready_.pop();
}

void AvrParser::tryParseLine(std::string_view line) {
    if (line.empty()) return;

    std::optional<codec::AdsMessage> msg;
    switch (line[0]) {
        case '*': msg = parseAvrStd (line.substr(1)); break;
        case '@': msg = parseAvrMlat(line.substr(1)); break;
        default:  break; // неизвестный формат — игнорируем
    }
    if (msg) ready_.push(std::move(*msg));
}

// ─── AVR Standard ────────────────────────────────────────────────────────────

std::optional<codec::AdsMessage> AvrParser::parseAvrStd(std::string_view hexPart) {
    auto bytes = parseHex(hexPart);
    if (!bytes) return std::nullopt;

    const size_t n = bytes->size();
    if (n != 2 && n != 7 && n != 14) return std::nullopt;

    codec::AdsMessage msg;
    msg.frameType = classifyFrame(n);
    msg.beastType = beastTypeFor(n);
    msg.frame     = std::move(*bytes);
    msg.timestamp = 0;
    msg.signal    = 0;
    msg.srcFormat = codec::Format::AvrStd;
    return msg;
}

// ─── AVR MLAT ────────────────────────────────────────────────────────────────

std::optional<codec::AdsMessage> AvrParser::parseAvrMlat(std::string_view hexPart) {
    // Минимум: 12 символов timestamp + 4 символа ModeAC
    if (hexPart.size() < 16) return std::nullopt;

    auto tsBytes = parseHex(hexPart.substr(0, 12));
    if (!tsBytes || tsBytes->size() != 6) return std::nullopt;

    auto frameBytes = parseHex(hexPart.substr(12));
    if (!frameBytes) return std::nullopt;

    const size_t n = frameBytes->size();
    if (n != 2 && n != 7 && n != 14) return std::nullopt;

    // Восстановление 48-бит timestamp (big-endian)
    uint64_t ts = 0;
    for (uint8_t byte : *tsBytes) ts = (ts << 8) | byte;

    codec::AdsMessage msg;
    msg.frameType = classifyFrame(n);
    msg.beastType = beastTypeFor(n);
    msg.frame     = std::move(*frameBytes);
    msg.timestamp = ts;
    msg.signal    = 0;
    msg.srcFormat = codec::Format::AvrMlat;
    return msg;
}

// ─── Вспомогательные функции ──────────────────────────────────────────────────

std::optional<std::vector<uint8_t>> AvrParser::parseHex(std::string_view hex) {
    if (hex.empty() || hex.size() % 2 != 0) return std::nullopt;

    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        auto fromHex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        const int hi = fromHex(hex[i]);
        const int lo = fromHex(hex[i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        result.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return result;
}

codec::FrameType AvrParser::classifyFrame(size_t bytes) noexcept {
    if (bytes == 2) return codec::FrameType::ModeAC;
    if (bytes == 7) return codec::FrameType::Short;
    return codec::FrameType::Long;
}

uint8_t AvrParser::beastTypeFor(size_t bytes) noexcept {
    if (bytes == 2) return '1';
    if (bytes == 7) return '2';
    return '3';
}

} // namespace avr
