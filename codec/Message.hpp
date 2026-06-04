#pragma once

#include <cstdint>
#include <vector>

namespace codec {

// Форматы wire-протоколов
enum class Format {
    Beast,    // Binary Beast (0x1A фреймирование с ESC-ESC экранированием)
    AvrStd,   // AVR Standard   (*HEXDATA;\n)
    AvrMlat,  // AVR with MLAT  (@TSHEXDATA;\n)
};

// Тип кадра Mode-S/Mode-AC
enum class FrameType {
    ModeAC,  // 2 байта
    Short,   // 7 байт  (56-бит Mode-S)
    Long,    // 14 байт (112-бит Mode-S)
};

// Общее внутреннее представление одного ADS-B/Mode-S сообщения.
// Не зависит от wire-формата. Недоступные поля обнуляются.
struct AdsMessage {
    FrameType            frameType  = FrameType::Long;
    uint8_t              beastType  = '3';  // оригинальный тип-байт Beast ('1'..'5')
    std::vector<uint8_t> frame;             // декодированный payload (2, 7 или 14 байт)
    uint64_t             timestamp  = 0;    // 48-бит MLAT (0 если недоступно)
    uint8_t              signal     = 0;    // уровень сигнала (0 если недоступно)
    Format               srcFormat  = Format::Beast;
};

} // namespace codec
