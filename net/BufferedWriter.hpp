#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stop_token>

#include "net/Poll.hpp"

namespace net {

// Fixed-size output buffer that accumulates encoded frames
// and sends them to the socket in one write() per flush.
//
// Typical benefit: reduces syscalls from N_packets/s to ~1-5/s.
class BufferedWriter {
public:
    static constexpr size_t kCapacity = 8192;

    explicit BufferedWriter(int fd) noexcept : fd_(fd) {}

    // Append data to internal buffer, flushing automatically when full.
    // Returns false on write error or stop request.
    [[nodiscard]] bool append(const uint8_t* data, size_t len,
                               std::stop_token st) noexcept {
        while (len > 0) {
            const size_t space = kCapacity - used_;
            if (space == 0) {
                if (!flush(st)) return false;
                continue;
            }
            const size_t chunk = (len < space) ? len : space;
            std::memcpy(buf_ + used_, data, chunk);
            used_ += chunk;
            data  += chunk;
            len   -= chunk;
        }
        return true;
    }

    // Write all buffered data to the socket in one syscall.
    [[nodiscard]] bool flush(std::stop_token st) noexcept {
        if (used_ == 0) return true;
        const bool ok = writeAll(fd_, buf_, used_, st);
        used_ = 0;
        return ok;
    }

    [[nodiscard]] bool hasData() const noexcept { return used_ > 0; }

private:
    int     fd_;
    size_t  used_ = 0;
    uint8_t buf_[kCapacity];
};

} // namespace net
