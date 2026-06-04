#include "net/Poll.hpp"
#include "compat/format.hpp"  // ← было: #include <format>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace net {

static PollResult doPoll(int fd, short events,
                          std::chrono::milliseconds timeout,
                          std::stop_token st) {
    using namespace std::chrono;
    const auto deadline = steady_clock::now() + timeout;

    while (!st.stop_requested()) {
        const auto remaining =
            duration_cast<milliseconds>(deadline - steady_clock::now());

        if (remaining <= milliseconds{0})
            return PollResult::Timeout;

        const int sliceMs = static_cast<int>(
            std::min(remaining, milliseconds{100}).count());

        struct pollfd pfd{ fd, events, 0 };
        const int ret = ::poll(&pfd, 1, sliceMs);

        if (ret > 0)                   return PollResult::Ready;
        if (ret < 0 && errno != EINTR) return PollResult::Error;
    }

    return PollResult::Stopped;
}

PollResult pollRead(int fd, std::chrono::milliseconds timeout, std::stop_token st) {
    return doPoll(fd, POLLIN, timeout, st);
}

PollResult pollWrite(int fd, std::chrono::milliseconds timeout, std::stop_token st) {
    return doPoll(fd, POLLOUT, timeout, st);
}

bool writeAll(int fd, const uint8_t* data, size_t len, std::stop_token st) {
    size_t written = 0;
    while (written < len) {
        if (st.stop_requested()) return false;

        const ssize_t n = ::write(fd, data + written, len - written);

        if (n > 0) {
            written += static_cast<size_t>(n);
        } else if (n == 0) {
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (pollWrite(fd, std::chrono::seconds{5}, st) != PollResult::Ready)
                    return false;
            } else {
                return false;
            }
        }
    }
    return true;
}

Socket connectTcp(const std::string& host, uint16_t port,
                  std::chrono::milliseconds timeout,
                  std::stop_token st) {
    if (st.stop_requested()) return Socket{};

    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const std::string portStr = std::to_string(port);
    struct addrinfo*  res     = nullptr;

    const int gaiErr = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (gaiErr != 0) {
        std::cerr << std::format("[net] getaddrinfo({}:{}): {}\n",
                                  host, port, ::gai_strerror(gaiErr));
        return Socket{};
    }

    struct Guard { addrinfo* p; ~Guard() { ::freeaddrinfo(p); } } guard{res};

    for (addrinfo* p = res; p != nullptr && !st.stop_requested(); p = p->ai_next) {
        int fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        Socket sock(fd);
        sock.setNonBlocking();

        const int rc = ::connect(fd, p->ai_addr, p->ai_addrlen);

        if (rc == 0) return sock;
        if (errno != EINPROGRESS) continue;

        if (pollWrite(fd, timeout, st) != PollResult::Ready) continue;

        int       soErr = 0;
        socklen_t soLen = sizeof(soErr);
        ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soErr, &soLen);

        if (soErr == 0) return sock;

        std::cerr << std::format("[net] connect({}:{}): {}\n",
                                  host, port, std::strerror(soErr));
    }

    return Socket{};
}

} // namespace net
