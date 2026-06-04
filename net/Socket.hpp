#pragma once

#include <string>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <utility>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

namespace net {

// RAII-обёртка над файловым дескриптором TCP-сокета.
class Socket {
public:
    Socket() = default;
    explicit Socket(int fd) noexcept : fd_(fd) {}

    Socket(Socket&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    Socket& operator=(Socket&& o) noexcept {
        if (this != &o) { doClose(); fd_ = std::exchange(o.fd_, -1); }
        return *this;
    }

    ~Socket() { doClose(); }

    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] int  fd()    const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    int release() noexcept { return std::exchange(fd_, -1); }

    void doClose() noexcept {
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    }

    void setNonBlocking() const {
        const int fl = ::fcntl(fd_, F_GETFL, 0);
        if (fl < 0 || ::fcntl(fd_, F_SETFL, fl | O_NONBLOCK) < 0)
            throw std::runtime_error(
                std::string("fcntl(O_NONBLOCK): ") + std::strerror(errno));
    }

    void setNoDelay() const noexcept {
        int yes = 1;
        ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    }

    void setReuseAddr() const noexcept {
        int yes = 1;
        ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    }

    void setSendBuffer(int bytes) const noexcept {
        ::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
    }

    // Создать слушающий TCP-сокет, привязанный к bindAddr:port.
    // Сокет переводится в неблокирующий режим.
    // Бросает std::runtime_error при ошибке.
    static Socket createListening(const std::string& bindAddr,
                                   uint16_t port, int backlog = 511);

private:
    int fd_ = -1;
};

} // namespace net
