#include "endpoints/OutputServer.hpp"
#include "compat/format.hpp"  // ← было: #include <format>

#include "beast/Protocol.hpp"
#include "codec/Transcoder.hpp"
#include "net/Poll.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <vector>

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

OutputServer::OutputServer(uint16_t port, MessageHub& hub,
                            codec::Format outputFormat, bool keepalive,
                            std::string bindAddr)
    : Endpoint(std::format("OutServer({} :{})", fmtName(outputFormat), port))
    , port_(port), hub_(hub), outputFormat_(outputFormat)
    , keepalive_(keepalive), bindAddr_(std::move(bindAddr))
{}

void OutputServer::run(std::stop_token st) {
    auto listenSock = net::Socket::createListening(bindAddr_, port_);
    std::cerr << std::format("[{}] listening\n", name_);

    std::vector<std::jthread> clients;

    while (!st.stop_requested()) {
        const auto pr = net::pollRead(listenSock.fd(), kAcceptPoll, st);
        if (pr != net::PollResult::Ready) continue;

        struct sockaddr_storage ss{};
        socklen_t ssLen = sizeof(ss);
        const int cfd = ::accept(listenSock.fd(),
                                  reinterpret_cast<sockaddr*>(&ss), &ssLen);
        if (cfd < 0) continue;

        char     ipStr[INET6_ADDRSTRLEN] = "?";
        uint16_t peerPort                = 0;
        if (ss.ss_family == AF_INET) {
            auto* s4 = reinterpret_cast<sockaddr_in*>(&ss);
            ::inet_ntop(AF_INET, &s4->sin_addr, ipStr, sizeof(ipStr));
            peerPort = ntohs(s4->sin_port);
        } else if (ss.ss_family == AF_INET6) {
            auto* s6 = reinterpret_cast<sockaddr_in6*>(&ss);
            ::inet_ntop(AF_INET6, &s6->sin6_addr, ipStr, sizeof(ipStr));
            peerPort = ntohs(s6->sin6_port);
        }
        std::string peer = std::format("{}:{}", ipStr, peerPort);
        std::cerr << std::format("[{}] accepted {}\n", name_, peer);

        auto queue = hub_.subscribe(std::format("{}/{}", name_, peer));

        clients.emplace_back(
            [this, fd = cfd, peer, q = queue](std::stop_token cst) mutable {
                net::Socket sock(fd);
                sock.setNonBlocking();
                sock.setNoDelay();
                sock.setSendBuffer(256 * 1024);
                handleClient(std::move(sock), peer, q, cst);
                hub_.unsubscribe(q);
                std::cerr << std::format("[{}] {} disconnected\n", name_, peer);
            }
        );
    }

    std::cerr << std::format("[{}] stopped\n", name_);
}

void OutputServer::handleClient(net::Socket sock, std::string /*peer*/,
                                 std::shared_ptr<SendQueue> queue,
                                 std::stop_token st) {
    using namespace std::chrono;

    const bool doKeepalive = keepalive_ && (outputFormat_ == codec::Format::Beast);
    auto       lastSend    = steady_clock::now();

    while (!st.stop_requested()) {
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
                if (!net::writeAll(sock.fd(), hb.data(), hb.size(), st)) break;
                lastSend = steady_clock::now();
            }
            continue;
        }

        const auto encoded = codec::encode(*msg, outputFormat_);
        if (encoded.empty()) continue;
        if (!net::writeAll(sock.fd(), encoded.data(), encoded.size(), st)) break;
        lastSend = steady_clock::now();
    }
}
