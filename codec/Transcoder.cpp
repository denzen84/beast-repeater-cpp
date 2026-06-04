// Исправлено: убрана опечатка #include "codec/Transcoder.cpp" (самовключение),
// добавлен #include <cstddef> для size_t.

#include "codec/Transcoder.hpp"

#include <cstddef>  // std::size_t

namespace codec {

namespace {

// ─── Вспомогательные функции ──────────────────────────────────────────────────

static char hexNibble(uint8_t n) {
    return n < 10 ? static_cast<char>('0' + n)
                  : static_cast<char>('A' + n - 10);
}

static void appendHex(std::vector<uint8_t>& out,
                      const uint8_t* data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(static_cast<uint8_t>(hexNibble(data[i] >> 4)));
        out.push_back(static_cast<uint8_t>(hexNibble(data[i] & 0x0F)));
    }
}

// Конфигурационный кадр Beast type-4/5 — нет представления в AVR-форматах
static bool isConfigFrame(const AdsMessage& msg) noexcept {
    return msg.beastType == '4' || msg.beastType == '5';
}

// ─── Beast encoder ────────────────────────────────────────────────────────────

static std::vector<uint8_t> encodeBeast(const AdsMessage& msg) {
    const uint64_t ts  = (msg.srcFormat == Format::AvrStd) ? 0 : msg.timestamp;
    const uint8_t  sig = (msg.srcFormat == Format::Beast)  ? msg.signal : 0;

    std::vector<uint8_t> out;
    out.reserve(2 + 7 + msg.frame.size() * 2);

    out.push_back(0x1A);
    out.push_back(msg.beastType);

    // 6-байтный timestamp big-endian с ESC-ESC экранированием
    for (int i = 5; i >= 0; --i) {
        const uint8_t b = static_cast<uint8_t>((ts >> (i * 8)) & 0xFF);
        if (b == 0x1A) out.push_back(0x1A);
        out.push_back(b);
    }

    // Signal byte
    if (sig == 0x1A) out.push_back(0x1A);
    out.push_back(sig);

    // Payload с ESC-ESC экранированием
    for (uint8_t b : msg.frame) {
        if (b == 0x1A) out.push_back(0x1A);
        out.push_back(b);
    }

    return out;
}

// ─── AVR Standard encoder ────────────────────────────────────────────────────

static std::vector<uint8_t> encodeAvrStd(const AdsMessage& msg) {
    if (isConfigFrame(msg)) return {};

    std::vector<uint8_t> out;
    out.reserve(2 + msg.frame.size() * 2 + 2);
    out.push_back('*');
    appendHex(out, msg.frame.data(), msg.frame.size());
    out.push_back(';');
    out.push_back('\n');
    return out;
}

// ─── AVR MLAT encoder ────────────────────────────────────────────────────────

static std::vector<uint8_t> encodeAvrMlat(const AdsMessage& msg) {
    if (isConfigFrame(msg)) return {};

    const uint64_t ts = (msg.srcFormat == Format::AvrStd) ? 0 : msg.timestamp;

    std::vector<uint8_t> out;
    out.reserve(1 + 12 + msg.frame.size() * 2 + 2);
    out.push_back('@');

    uint8_t tsBuf[6];
    for (int i = 5; i >= 0; --i)
        tsBuf[5 - i] = static_cast<uint8_t>((ts >> (i * 8)) & 0xFF);
    appendHex(out, tsBuf, 6);

    appendHex(out, msg.frame.data(), msg.frame.size());
    out.push_back(';');
    out.push_back('\n');
    return out;
}

} // anonymous namespace

// ─── Публичный API ────────────────────────────────────────────────────────────

std::vector<uint8_t> encode(const AdsMessage& msg, Format targetFmt) {
    switch (targetFmt) {
        case Format::Beast:   return encodeBeast(msg);
        case Format::AvrStd:  return encodeAvrStd(msg);
        case Format::AvrMlat: return encodeAvrMlat(msg);
    }
    return {};
}

} // namespace codec
