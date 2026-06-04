#pragma once

#include "codec/Message.hpp"
#include <vector>
#include <cstdint>

namespace codec {

// Кодировать AdsMessage в целевой wire-формат.
// Правила транскодирования:
//   Beast   → Beast   : полная копия (timestamp + signal сохраняются)
//   Beast   → AvrStd  : только frame (timestamp/signal отбрасываются)
//   Beast   → AvrMlat : frame + timestamp (signal отбрасывается)
//   AvrStd  → Beast   : frame, timestamp=0, signal=0
//   AvrStd  → AvrStd  : копия
//   AvrStd  → AvrMlat : frame, timestamp=0
//   AvrMlat → Beast   : frame + timestamp, signal=0
//   AvrMlat → AvrStd  : только frame
//   AvrMlat → AvrMlat : полная копия
//
// Возвращает пустой вектор для кадров без представления в целевом формате
// (например, Beast type-4/type-5 конфигурационные кадры → AVR-форматы).
std::vector<uint8_t> encode(const AdsMessage& msg, Format targetFmt);

} // namespace codec
