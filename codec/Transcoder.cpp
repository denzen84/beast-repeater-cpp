#include "codec/Transcoder.hpp"
#include "codec/HexTables.hpp"

#include <cstddef>

namespace codec {
namespace {

[[nodiscard]] inline bool isConfig(const AdsMessage& m) noexcept {
    return m.beastType == '4' || m.beastType == '5';
}

// ─── Beast ────────────────────────────────────────────────────────────────────

static size_t doBeast(const AdsMessage& m, uint8_t* o) noexcept {
    const uint64_t ts  = (m.srcFormat == Format::AvrStd) ? 0 : m.timestamp;
    const uint8_t  sig = (m.srcFormat == Format::Beast)  ? m.signal : 0;

    uint8_t* p = o;
    *p++ = 0x1A;
    *p++ = m.beastType;

    // 6-byte big-endian timestamp with ESC-ESC escaping
    for (int i = 5; i >= 0; --i) {
        const uint8_t b = static_cast<uint8_t>((ts >> (i * 8)) & 0xFF);
        if (b == 0x1A) *p++ = 0x1A;
        *p++ = b;
    }
    // Signal byte
    if (sig == 0x1A) *p++ = 0x1A;
    *p++ = sig;
    // Payload with ESC-ESC escaping
    for (uint8_t i = 0; i < m.frameLen; ++i) {
        const uint8_t b = m.frame[i];
        if (b == 0x1A) *p++ = 0x1A;
        *p++ = b;
    }
    return static_cast<size_t>(p - o);
}

// ─── AVR Standard ─────────────────────────────────────────────────────────────

static size_t doAvrStd(const AdsMessage& m, uint8_t* o) noexcept {
    if (isConfig(m)) return 0;
    uint8_t* p = o;
    *p++ = '*';
    hex::encodeBytes(m.frame.data(), m.frameLen, p);
    p += m.frameLen * 2;
    *p++ = ';';
    *p++ = '\n';
    return static_cast<size_t>(p - o);
}

// ─── AVR MLAT ─────────────────────────────────────────────────────────────────

static size_t doAvrMlat(const AdsMessage& m, uint8_t* o) noexcept {
    if (isConfig(m)) return 0;
    const uint64_t ts = (m.srcFormat == Format::AvrStd) ? 0 : m.timestamp;

    uint8_t* p = o;
    *p++ = '@';
    // 6-byte timestamp as 12 hex chars (big-endian)
    for (int i = 5; i >= 0; --i) {
        const uint8_t b = static_cast<uint8_t>((ts >> (i * 8)) & 0xFF);
        *p++ = hex::kHi[b];
        *p++ = hex::kLo[b];
    }
    hex::encodeBytes(m.frame.data(), m.frameLen, p);
    p += m.frameLen * 2;
    *p++ = ';';
    *p++ = '\n';
    return static_cast<size_t>(p - o);
}

} // anonymous namespace

size_t encode(const AdsMessage& msg, Format targetFmt, uint8_t* out) noexcept {
    switch (targetFmt) {
        case Format::Beast:   return doBeast  (msg, out);
        case Format::AvrStd:  return doAvrStd (msg, out);
        case Format::AvrMlat: return doAvrMlat(msg, out);
    }
    return 0;
}

} // namespace codec
