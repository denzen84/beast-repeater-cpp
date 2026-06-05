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
        // ── 1. Sync byte ──────────────────────────────────────────────────
        if (buf_[pos] != ESC) { ++pos; continue; }

        // ── 2. Type byte ──────────────────────────────────────────────────
        if (pos + 1 >= buf_.size()) break;
        const uint8_t type     = buf_[pos + 1];
        const int     expected = bodySize(type);
        if (expected < 0) { ++pos; continue; }

        // ── 3. Find frame boundary (count unescaped body bytes) ───────────
        size_t scan    = pos + 2;
        int    count   = 0;
        bool   needMore = false;
        bool   corrupt  = false;

        while (count < expected) {
            if (scan >= buf_.size())            { needMore = true;  break; }
            if (buf_[scan] == ESC) {
                if (scan + 1 >= buf_.size())    { needMore = true;  break; }
                if (buf_[scan + 1] == ESC)      { scan += 2; ++count; }
                else                            { pos = scan; corrupt = true; break; }
            } else {
                ++scan; ++count;
            }
        }
        if (corrupt)  continue;
        if (needMore) break;

        // ── 4. Extract directly into AdsMessage (no intermediate vector) ──
        codec::AdsMessage msg;
        msg.srcFormat = codec::Format::Beast;
        msg.beastType = type;

        const uint8_t* src = buf_.data() + pos + 2;
        const uint8_t* end = buf_.data() + scan;

        // 6-byte big-endian MLAT timestamp (unescape inline)
        uint64_t ts = 0;
        for (int i = 0; i < 6 && src < end; ++i) {
            uint8_t b;
            if (*src == ESC) { ++src; b = *src; } else { b = *src; }
            ++src;
            ts = (ts << 8) | b;
        }
        msg.timestamp = ts;

        // Signal byte
        if (src < end) {
            if (*src == ESC) { ++src; msg.signal = *src; }
            else             {        msg.signal = *src; }
            ++src;
        }

        // Frame payload
        msg.frameLen = 0;
        while (src < end &&
               msg.frameLen < static_cast<uint8_t>(codec::AdsMessage::kMaxFrame)) {
            uint8_t b;
            if (*src == ESC) { ++src; b = *src; } else { b = *src; }
            ++src;
            msg.frame[msg.frameLen++] = b;
        }

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
