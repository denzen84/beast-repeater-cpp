#include "endpoints/InputConnector.hpp"
#include "compat/format.hpp"  // ← было: #include <format>

#include "net/Poll.hpp"

#include <iostream>
#include <unistd.h>

namespace {
const char* fmtName(codec::Format f) noexcept {
    switch (f) {
        case codec::Format::Beast:   return "Beast Binary";
        case codec::Format::AvrStd:  return "AVR Standard";
        case codec::Format::AvrMlat: return "AVR MLAT";
    }
    return "?";
}
} // namespace

InputConnector::InputConnector(std::string host, uint16_t port, MessageHub& hub)
    : Endpoint(std::format("InConn({}:{})", host, port))
    , host_(std::move(host)), port_(port), hub_(hub)
{}

void InputConnector::run(std::stop_token st) {
    std::cerr << std::format("[{}] started\n", name_);

    while (!st.stop_requested()) {
        std::cerr << std::format("[{}] connecting...\n", name_);

        auto sock = net::connectTcp(host_, port_, kConnectTimeout, st);
        if (!sock.valid()) {
            std::cerr << std::format("[{}] failed, retry in {}s\n",
                                      name_, kReconnectDelay.count() / 1000);
            net::sleepFor(kReconnectDelay, st);
            continue;
        }

        sock.setNoDelay();
        sock.setSendBuffer(64 * 1024);
        std::cerr << std::format("[{}] connected\n", name_);
        handleConnection(std::move(sock), st);
        std::cerr << std::format("[{}] disconnected\n", name_);

        if (!st.stop_requested())
            net::sleepFor(kReconnectDelay, st);
    }

    std::cerr << std::format("[{}] stopped\n", name_);
}

void InputConnector::handleConnection(net::Socket sock, std::stop_token st) {
    AutoDetectParser parser;
    uint8_t          buf[65536];
    bool             formatLogged = false;

    while (!st.stop_requested()) {
        const auto pr = net::pollRead(sock.fd(), kReadTimeout, st);

        if (pr == net::PollResult::Stopped) break;
        if (pr == net::PollResult::Error)   break;
        if (pr == net::PollResult::Timeout) {
            std::cerr << std::format("[{}] read timeout\n", name_);
            break;
        }

        const ssize_t n = ::read(sock.fd(), buf, sizeof(buf));
        if (n <= 0) break;

        parser.feed({ buf, static_cast<size_t>(n) });

        if (!formatLogged && parser.detected()) {
            std::cerr << std::format("[{}] input format detected: {}\n",
                                      name_, fmtName(parser.format()));
            formatLogged = true;
        }

        while (parser.hasMessage())
            hub_.publish(std::make_shared<codec::AdsMessage>(parser.pop()));
    }

    parser.reset();
}
