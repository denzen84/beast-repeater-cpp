#pragma once

#include "beast/Protocol.hpp"
#include "avr/Protocol.hpp"
#include "codec/Message.hpp"

#include <cstdint>
#include <span>
#include <type_traits>
#include <variant>     // ← обязателен для std::variant

// Auto-detecting parser using std::variant instead of virtual dispatch.
//
// Format detection on first meaningful byte:
//   0x1A  → Beast Binary
//   '*'   → AVR Standard  (*HEXDATA;)
//   '@'   → AVR MLAT      (@TSHEXDATA;)
//
// std::visit generates direct calls or a small inline branch table;
// no vtable overhead.
class AutoDetectParser {
    using Var = std::variant<std::monostate,       // not yet detected
                             beast::BeastParser,
                             avr::AvrParser>;

    Var           inner_;
    codec::Format detectedFmt_ = codec::Format::Beast;

    // Convenience: is T the "not yet detected" monostate?
    template<typename T>
    static constexpr bool isMono =
        std::is_same_v<std::decay_t<T>, std::monostate>;

public:
    AutoDetectParser() = default;

    void feed(std::span<const uint8_t> data) {
        if (!std::holds_alternative<std::monostate>(inner_)) {
            std::visit([&data](auto& p) {
                if constexpr (!isMono<decltype(p)>) p.feed(data);
            }, inner_);
            return;
        }
        // Detect format from first meaningful byte
        for (size_t i = 0; i < data.size(); ++i) {
            const uint8_t b = data[i];
            if      (b == 0x1A) {
                inner_.emplace<beast::BeastParser>();
                detectedFmt_ = codec::Format::Beast;
            } else if (b == '*') {
                inner_.emplace<avr::AvrParser>();
                detectedFmt_ = codec::Format::AvrStd;
            } else if (b == '@') {
                inner_.emplace<avr::AvrParser>();
                detectedFmt_ = codec::Format::AvrMlat;
            } else {
                continue;  // skip whitespace / noise
            }

            // Feed the remaining bytes (including the trigger byte) to the parser
            const auto rem = data.subspan(i);
            std::visit([&rem](auto& p) {
                if constexpr (!isMono<decltype(p)>) p.feed(rem);
            }, inner_);
            return;
        }
    }

    [[nodiscard]] bool hasMessage() const noexcept {
        return std::visit([](const auto& p) -> bool {
            if constexpr (isMono<decltype(p)>) return false;
            else                               return p.hasMessage();
        }, inner_);
    }

    [[nodiscard]] codec::AdsMessage pop() {
        codec::AdsMessage m{};
        std::visit([&m](auto& p) {
            if constexpr (!isMono<decltype(p)>) m = p.pop();
        }, inner_);
        return m;
    }

    void reset() { inner_.emplace<std::monostate>(); }

    [[nodiscard]] bool          detected() const noexcept {
        return !std::holds_alternative<std::monostate>(inner_);
    }
    [[nodiscard]] codec::Format format()   const noexcept { return detectedFmt_; }
};
