#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Compile-time hex lookup tables.
// Encoding: O(1) per byte  (was: branch+arithmetic)
// Decoding: O(1) per char  (was: if-chain)
namespace hex {

namespace detail {

constexpr std::array<uint8_t, 256> makeHi() noexcept {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[static_cast<size_t>(i)] =
            static_cast<uint8_t>("0123456789ABCDEF"[i >> 4]);
    return t;
}
constexpr std::array<uint8_t, 256> makeLo() noexcept {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[static_cast<size_t>(i)] =
            static_cast<uint8_t>("0123456789ABCDEF"[i & 0xF]);
    return t;
}
constexpr std::array<int8_t, 256> makeFrom() noexcept {
    std::array<int8_t, 256> t{};
    t.fill(-1);
    for (int i = '0'; i <= '9'; ++i)
        t[static_cast<size_t>(i)] = static_cast<int8_t>(i - '0');
    for (int i = 'A'; i <= 'F'; ++i)
        t[static_cast<size_t>(i)] = static_cast<int8_t>(i - 'A' + 10);
    for (int i = 'a'; i <= 'f'; ++i)
        t[static_cast<size_t>(i)] = static_cast<int8_t>(i - 'a' + 10);
    return t;
}

} // namespace detail

// byte → high nibble ASCII  (0xAB → 'A')
inline constexpr std::array<uint8_t, 256> kHi   = detail::makeHi();
// byte → low  nibble ASCII  (0xAB → 'B')
inline constexpr std::array<uint8_t, 256> kLo   = detail::makeLo();
// hex ASCII → nibble value, -1 for non-hex
inline constexpr std::array<int8_t,  256> kFrom = detail::makeFrom();

// Encode n bytes → 2n hex chars into dst
inline void encodeBytes(const uint8_t* src,
                         uint8_t n,
                         uint8_t* dst) noexcept {
    for (uint8_t i = 0; i < n; ++i) {
        dst[2 * i]     = kHi[src[i]];
        dst[2 * i + 1] = kLo[src[i]];
    }
}

// Decode 2n hex chars → n bytes into dst.
// Returns n on success, 0 on any invalid character.
inline uint8_t decodeBytes(const uint8_t* src,
                            uint8_t n,
                            uint8_t* dst) noexcept {
    for (uint8_t i = 0; i < n; ++i) {
        const int8_t hi = kFrom[src[2 * i]];
        const int8_t lo = kFrom[src[2 * i + 1]];
        if (hi < 0 || lo < 0) return 0;
        dst[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return n;
}

} // namespace hex
