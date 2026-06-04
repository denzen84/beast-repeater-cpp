#include "net/Socket.hpp"
#include "compat/format.hpp"  // ← было: #include <format>

namespace net {

Socket Socket::createListening(const std::string& bindAddr,
                                uint16_t port, int backlog) {
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    const std::string portStr = std::to_string(port);
    struct addrinfo*  res     = nullptr;

    const int err = ::getaddrinfo(
        bindAddr.empty() ? nullptr : bindAddr.c_str(),
        portStr.c_str(), &hints, &res);

    if (err != 0)
        throw std::runtime_error(
            std::format("getaddrinfo({}:{}): {}", bindAddr, port,
                        ::gai_strerror(err)));

    struct Guard {
        addrinfo* p;
        ~Guard() { if (p) ::freeaddrinfo(p); }
    } guard{res};

    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        int fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        Socket sock(fd);
        sock.setReuseAddr();

        if (p->ai_family == AF_INET6) {
            int on = 1;
            ::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        }

        if (::bind(fd, p->ai_addr, p->ai_addrlen) != 0) continue;
        if (::listen(fd, backlog) != 0)                  continue;

        sock.setNonBlocking();
        return sock;
    }

    throw std::runtime_error(
        std::format("Failed to listen on {}:{}", bindAddr, port));
}

} // namespace net
