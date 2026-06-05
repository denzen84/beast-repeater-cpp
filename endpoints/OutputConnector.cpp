#include "endpoints/OutputConnector.hpp"
#include "compat/format.hpp"

#include "beast/Protocol.hpp"
#include "codec/Transcoder.hpp"
#include "net/BufferedWriter.hpp"
#include "net/Poll.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

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

OutputConnector::OutputConnector(std::string host, uint16_t port,
                                  MessageHub& hub,
                                  codec::Format outputFormat,
                                  bool keepalive)
    : Endpoint(std::format("OutConn({} {}:{})", fmtName(outputFormat), host, port))
    , host_(std::move(host)), port_(port), hub_(hub)
    , outputFormat_(outputFormat), keepalive_(keepalive)
{}

void OutputConnector::run(std::stop_token st) {
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
        sock.setSendBuffer(256 * 1024);
        std::cerr << std::format("[{}] connected\n", name_);

        auto queue = hub_.subscribe(name_);
        handleConnection(std::move(sock), queue, st);
        hub_.unsubscribe(queue);

        std::cerr << std::format("[{}] disconnected\n", name_);
        if (!st.stop_requested())
            net::sleepFor(kReconnectDelay, st);
    }

    std::cerr << std::format("[{}] stopped\n", name_);
}

void OutputConnector::handleConnection(net::Socket sock,
                                        std::shared_ptr<SendQueue> queue,
                                        std::stop_token st) {
    using namespace std::chrono;

    const bool doKeepalive = keepalive_ && (outputFormat_ == codec::Format::Beast);
    auto       lastSend    = steady_clock::now();

    // Output buffer: accumulates encoded frames → one write() per flush
    net::BufferedWriter writer(sock.fd());

    // Stack-allocated encode target: zero heap allocations per message
    uint8_t encodeBuf[codec::kMaxEncodedSize];

    while (!st.stop_requested()) {
        // Compute wait time (respecting keepalive interval)
        milliseconds waitTime;
        if (doKeepalive) {
            const auto elapsed =
                duration_cast<milliseconds>(steady_clock::now() - lastSend);
            waitTime = std::max(milliseconds{0}, kHeartbeatInterval - elapsed);
        } else {
            waitTime = kIdleTimeout;
        }

        std::shared_ptr<codec::AdsMessage> msg;
        const auto result = queue->pop(msg, waitTime, st);

        if (result == SendQueue::PopResult::Stopped) break;

        if (result == SendQueue::PopResult::Timeout) {
            if (doKeepalive) {
                const auto hb = beast::heartbeatBytes();
                if (!writer.append(hb.data(), hb.size(), st)) break;
                lastSend = steady_clock::now();
            }
            if (!writer.flush(st)) break;
            continue;
        }

        // ── Batch: encode the current message and drain any others ────────
        // After the first (blocking) pop, we do non-blocking tryPop() in a
        // tight loop. This processes a whole burst of packets in one thread
        // timeslice instead of waking up once per message.
        bool ok = true;
        do {
            const size_t n = codec::encode(*msg, outputFormat_, encodeBuf);
            if (n > 0) {
                ok = writer.append(encodeBuf, n, st);
                if (!ok) break;
            }
        } while (ok && !st.stop_requested() && queue->tryPop(msg));

        if (!ok) break;
        if (!writer.flush(st)) break;   // one syscall for the whole batch
        lastSend = steady_clock::now();
    }
}
