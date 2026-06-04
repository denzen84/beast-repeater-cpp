#include "beast/Protocol.hpp"

namespace beast {

void BeastParser::feed(std::span<const uint8_t> data) {
    buf_.insert(buf_.end(), data.begin(), data.end());
    process();
}

void BeastParser::reset() {
    buf_.clear();
    while (!ready_.empty()) ready_.pop();
}

codec::AdsMessage BeastParser::pop() {
    codec::AdsMessage m = std::move(ready_.front());
    ready_.pop();
    return m;
}

void BeastParser::process() {
    size_t pos = 0;

    while (pos < buf_.size()) {
        // ── 1. Синхробайт ─────────────────────────────────────────────────
        if (buf_[pos] != ESC) { ++pos; continue; }

        // ── 2. Байт типа ──────────────────────────────────────────────────
        if (pos + 1 >= buf_.size()) break;
        const uint8_t type     = buf_[pos + 1];
        const int     expected = bodySize(type);
        if (expected < 0) { ++pos; continue; }

        // ── 3. Сканирование тела с подсчётом логических байт ───────────────
        size_t scan    = pos + 2;
        int    count   = 0;
        bool   needMore = false;
        bool   corrupt  = false;

        while (count < expected) {
            if (scan >= buf_.size()) { needMore = true; break; }
            if (buf_[scan] == ESC) {
                if (scan + 1 >= buf_.size()) { needMore = true; break; }
                if (buf_[scan + 1] == ESC)   { scan += 2; ++count; }
                else                         { pos = scan; corrupt = true; break; }
            } else {
                ++scan; ++count;
            }
        }
        if (corrupt)  continue;
        if (needMore) break;

        // ── 4. Удаление экранирования ─────────────────────────────────────
        std::vector<uint8_t> body;
        body.reserve(static_cast<size_t>(expected));
        for (size_t i = pos + 2; i < scan; ) {
            if (buf_[i] == ESC) { body.push_back(buf_[i + 1]); i += 2; }
            else                { body.push_back(buf_[i]);     ++i;    }
        }
        // Структура тела: [0..5]=timestamp  [6]=signal  [7..end]=payload

        // ── 5. Формирование AdsMessage ────────────────────────────────────
        codec::AdsMessage msg;
        msg.srcFormat = codec::Format::Beast;
        msg.beastType = type;

        // Timestamp: big-endian 48 бит
        uint64_t ts = 0;
        for (int i = 0; i < 6; ++i)
            ts = (ts << 8) | body[static_cast<size_t>(i)];
        msg.timestamp = ts;
        msg.signal    = body[6];

        msg.frame.assign(body.begin() + 7, body.end());

        switch (type) {
            case '1': msg.frameType = codec::FrameType::ModeAC; break;
            case '2': msg.frameType = codec::FrameType::Short;  break;
            default:  msg.frameType = codec::FrameType::Long;   break;
        }

        ready_.push(std::move(msg));
        pos = scan;
    }

    if (pos > 0)
        buf_.erase(buf_.begin(),
                   buf_.begin() + static_cast<ptrdiff_t>(pos));

    if (buf_.size() > kMaxBuf)
        buf_.erase(buf_.begin(),
                   buf_.begin() + static_cast<ptrdiff_t>(buf_.size() - kMaxBuf / 2));
}

} // namespace beast
