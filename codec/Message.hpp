#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace codec {

enum class Format    : uint8_t { Beast, AvrStd, AvrMlat };
enum class FrameType : uint8_t { ModeAC, Short, Long };

// Common decoded ADS-B/Mode-S message.
//
// KEY CHANGE vs. previous version: frame[] is now an inline array (14 bytes).
// No heap allocation per message → eliminates ~80% of per-packet allocations.
struct AdsMessage {
    static constexpr uint8_t kMaxFrame = 14;

    FrameType               frameType = FrameType::Long;
    uint8_t                 beastType = '3';     // Beast wire type '1'..'5'
    uint8_t                 frameLen  = 0;       // actual bytes in frame[]
    std::array<uint8_t, 14> frame{};             // inline payload (no heap!)
    uint64_t                timestamp = 0;       // 48-bit MLAT (0 if N/A)
    uint8_t                 signal    = 0;       // signal level (0 if N/A)
    Format                  srcFormat = Format::Beast;
};

// Maximum encoded wire size for any single frame (worst-case Beast escaping):
//   Beast:    2 + (6+1+14)*2 = 44 bytes
//   AvrMlat:  1 + 12 + 28 + 2 = 43 bytes
// 64 gives a comfortable margin → fits on the stack in callers.
inline constexpr size_t kMaxEncodedSize = 64;

} // namespace codec
