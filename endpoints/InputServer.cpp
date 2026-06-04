#include "endpoints/InputServer.hpp"
#include "compat/format.hpp"  // ← было: #include <format>

#include "net/Poll.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
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

InputServer::InputServer(uint16_t port, MessageHub& hub, std::string bindAddr)
    : Endpoint(std::format("InServer(:{})", port))
    , port_(port), hub_(hub), bindAddr_(std::move(bindAddr))
{}

void InputServer::run(std::stop_token st) {
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

        clients.emplace_back(
            [this, fd = cfd, peer](std::stop_token cst) {
                net::Socket sock(fd);
                sock.setNonBlocking();
                sock.setNoDelay();
                handleClient(std::move(sock), peer, cst);
                std::cerr << std::format("[{}] {} disconnected\n", name_, peer);
            }
        );
    }

    std::cerr << std::format("[{}] stopped\n", name_);
}

void InputServer::handleClient(net::Socket sock, std::string peer,
                                std::stop_token st) {
    AutoDetectParser parser;
    uint8_t          buf[65536];
    bool             formatLogged = false;

    while (!st.stop_requested()) {
        const auto pr = net::pollRead(sock.fd(), kReadTimeout, st);

        if (pr == net::PollResult::Stopped) break;
        if (pr == net::PollResult::Error)   break;
        if (pr == net::PollResult::Timeout) {
            std::cerr << std::format("[{}] {} read timeout\n", name_, peer);
            break;
        }

        const ssize_t n = ::read(sock.fd(), buf, sizeof(buf));
        if (n <= 0) break;

        parser.feed({ buf, static_cast<size_t>(n) });

        if (!formatLogged && parser.detected()) {
            std::cerr << std::format("[{}] {} input format: {}\n",
                                      name_, peer, fmtName(parser.format()));
            formatLogged = true;
        }

        while (parser.hasMessage())
            hub_.publish(std::make_shared<codec::AdsMessage>(parser.pop()));
    }

    parser.reset();
}
