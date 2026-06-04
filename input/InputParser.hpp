#pragma once

#include "beast/Protocol.hpp"
#include "avr/Protocol.hpp"
#include "codec/Message.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

// ─── Абстрактный интерфейс ────────────────────────────────────────────────────

class InputParser {
public:
    virtual ~InputParser() = default;

    virtual void              feed(std::span<const uint8_t> data) = 0;
    [[nodiscard]] virtual bool hasMessage() const noexcept = 0;
    virtual codec::AdsMessage  pop()  = 0;
    virtual void              reset() = 0;
    [[nodiscard]] virtual codec::Format format() const noexcept = 0;
};

// ─── Обёртка над Beast-парсером ───────────────────────────────────────────────

class BeastInputParser final : public InputParser {
public:
    void feed(std::span<const uint8_t> data) override { parser_.feed(data); }
    [[nodiscard]] bool hasMessage() const noexcept override { return parser_.hasMessage(); }
    codec::AdsMessage  pop()  override { return parser_.pop(); }
    void               reset() override { parser_.reset(); }
    [[nodiscard]] codec::Format format() const noexcept override {
        return codec::Format::Beast;
    }
private:
    beast::BeastParser parser_;
};

// ─── Обёртка над AVR-парсером ─────────────────────────────────────────────────

class AvrInputParser final : public InputParser {
public:
    explicit AvrInputParser(codec::Format fmt) : fmt_(fmt) {}

    void feed(std::span<const uint8_t> data) override { parser_.feed(data); }
    [[nodiscard]] bool hasMessage() const noexcept override { return parser_.hasMessage(); }
    codec::AdsMessage  pop()  override { return parser_.pop(); }
    void               reset() override { parser_.reset(); }
    [[nodiscard]] codec::Format format() const noexcept override { return fmt_; }
private:
    avr::AvrParser parser_;
    codec::Format  fmt_;
};

// ─── Авто-определение формата ─────────────────────────────────────────────────
// Анализирует первый значимый байт входного потока:
//   0x1A  → Beast Binary
//   '*'   → AVR Standard
//   '@'   → AVR MLAT
// Все последующие данные делегируются созданному конкретному парсеру.

class AutoDetectParser final : public InputParser {
public:
    void feed(std::span<const uint8_t> data) override {
        if (inner_) { inner_->feed(data); return; }

        for (size_t i = 0; i < data.size(); ++i) {
            const uint8_t b = data[i];

            if      (b == 0x1A) inner_ = std::make_unique<BeastInputParser>();
            else if (b == '*')  inner_ = std::make_unique<AvrInputParser>(codec::Format::AvrStd);
            else if (b == '@')  inner_ = std::make_unique<AvrInputParser>(codec::Format::AvrMlat);
            else continue; // пробельные символы или шум — пропускаем

            // Отдаём оставшиеся байты (включая триггерный) конкретному парсеру
            inner_->feed(data.subspan(i));
            return;
        }
    }

    [[nodiscard]] bool hasMessage() const noexcept override {
        return inner_ && inner_->hasMessage();
    }

    codec::AdsMessage pop() override { return inner_->pop(); }

    void reset() override { inner_.reset(); }

    [[nodiscard]] codec::Format format() const noexcept override {
        return inner_ ? inner_->format() : codec::Format::Beast;
    }

    // Возвращает true как только формат определён
    [[nodiscard]] bool detected() const noexcept { return inner_ != nullptr; }

private:
    std::unique_ptr<InputParser> inner_;
};
