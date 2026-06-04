#pragma once

#include "codec/Message.hpp"
#include "core/MessageHub.hpp"
#include "endpoints/Endpoint.hpp"
#include "net/Socket.hpp"

#include <chrono>
#include <memory>
#include <string>

class OutputServer final : public Endpoint {
public:
    OutputServer(uint16_t port, MessageHub& hub,
                 codec::Format outputFormat,
                 bool keepalive,
                 std::string bindAddr = "0.0.0.0");
    ~OutputServer() override { stop(); }

protected:
    void run(std::stop_token st) override;

private:
    void handleClient(net::Socket sock, std::string peer,
                      std::shared_ptr<SendQueue> queue,
                      std::stop_token st);

    uint16_t      port_;
    MessageHub&   hub_;
    codec::Format outputFormat_;
    bool          keepalive_;
    std::string   bindAddr_;

    static constexpr auto kAcceptPoll        = std::chrono::milliseconds{500};
    static constexpr auto kHeartbeatInterval = std::chrono::milliseconds{60'000};
    static constexpr auto kIdleTimeout       = std::chrono::milliseconds{30'000};
};
