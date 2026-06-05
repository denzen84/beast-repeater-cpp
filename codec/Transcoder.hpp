#pragma once

#include "codec/Message.hpp"
#include <cstddef>
#include <cstdint>

namespace codec {

// Encode msg to targetFmt, writing directly into outBuf.
//
// outBuf MUST point to at least kMaxEncodedSize bytes (stack-allocate it).
// Returns bytes written, or 0 if the frame has no representation in the
// target format (Beast type-4/5 config frames → AVR outputs).
//
// No heap allocation.
size_t encode(const AdsMessage& msg, Format targetFmt,
              uint8_t* outBuf) noexcept;

} // namespace codec
