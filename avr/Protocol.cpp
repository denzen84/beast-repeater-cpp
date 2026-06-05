#include "avr/Protocol.hpp"
#include "codec/HexTables.hpp"

namespace avr {

void AvrParser::feed(std::span<const uint8_t> data) {
    for (const uint8_t b : data) {
        if (b == ';') {
            if (!lineBuf_.empty()) {
                tryParseLine(lineBuf_);
                lineBuf_.clear();
            }
        } else if (b == '\r' || b == '\n') {
            // ignore line endings — frames are delimited by ';'
        } else {
            if (lineBuf_.size() < kMaxLine)
                lineBuf_.push_back(static_cast<char>(b));
            else
                lineBuf_.clear(); // overflow → discard and resync
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

    codec::AdsMessage msg;
    bool ok = false;

    if (line[0] == '*') {
        // ── AVR Standard: *HEXDATA ──────────────────────────────────────────
        const uint8_t n = parseHexInto(line.substr(1),
                                        msg.frame.data(),
                                        codec::AdsMessage::kMaxFrame);
        if (n == 2 || n == 7 || n == 14) {
            msg.frameLen  = n;
            msg.frameType = classifyFrame(n);
            msg.beastType = beastTypeFor(n);
            msg.srcFormat = codec::Format::AvrStd;
            msg.timestamp = 0;
            msg.signal    = 0;
            ok = true;
        }

    } else if (line[0] == '@') {
        // ── AVR MLAT: @TSHEXDATA  (TS = 12 hex chars = 6 bytes) ────────────
        const auto rest = line.substr(1);
        if (rest.size() >= 16) { // minimum: 12 ts + 4 Mode-AC
            uint8_t tsBuf[6];
            if (parseHexInto(rest.substr(0, 12), tsBuf, 6) == 6) {
                // Reconstruct 48-bit timestamp (big-endian)
                uint64_t ts = 0;
                for (int i = 0; i < 6; ++i) ts = (ts << 8) | tsBuf[i];

                const uint8_t n = parseHexInto(rest.substr(12),
                                                msg.frame.data(),
                                                codec::AdsMessage::kMaxFrame);
                if (n == 2 || n == 7 || n == 14) {
                    msg.frameLen  = n;
                    msg.frameType = classifyFrame(n);
                    msg.beastType = beastTypeFor(n);
                    msg.srcFormat = codec::Format::AvrMlat;
                    msg.timestamp = ts;
                    msg.signal    = 0;
                    ok = true;
                }
            }
        }
    }

    if (ok) ready_.push(std::move(msg));
}

// ─── Hex decode using precomputed lookup table ────────────────────────────────

uint8_t AvrParser::parseHexInto(std::string_view hex,
                                  uint8_t* dst,
                                  uint8_t maxLen) noexcept {
    if (hex.empty() || hex.size() % 2 != 0) return 0;
    const auto n = static_cast<uint8_t>(hex.size() / 2);
    if (n > maxLen) return 0;

    for (uint8_t i = 0; i < n; ++i) {
        const int8_t hi = hex::kFrom[static_cast<uint8_t>(hex[2 * i])];
        const int8_t lo = hex::kFrom[static_cast<uint8_t>(hex[2 * i + 1])];
        if (hi < 0 || lo < 0) return 0;
        dst[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return n;
}

codec::FrameType AvrParser::classifyFrame(uint8_t n) noexcept {
    if (n == 2) return codec::FrameType::ModeAC;
    if (n == 7) return codec::FrameType::Short;
    return codec::FrameType::Long;
}

uint8_t AvrParser::beastTypeFor(uint8_t n) noexcept {
    if (n == 2) return '1';
    if (n == 7) return '2';
    return '3';
}

} // namespace avr
