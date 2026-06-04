#pragma once

#include "codec/Message.hpp"
#include "core/MessageHub.hpp"
#include "endpoints/Endpoint.hpp"
#include "net/Socket.hpp"

#include <chrono>
#include <memory>
#include <string>

class OutputConnector final : public Endpoint {
public:
    OutputConnector(std::string host, uint16_t port,
                    MessageHub& hub,
                    codec::Format outputFormat,
                    bool keepalive);
    ~OutputConnector() override { stop(); }

protected:
    void run(std::stop_token st) override;

private:
    void handleConnection(net::Socket sock,
                          std::shared_ptr<SendQueue> queue,
                          std::stop_token st);

    std::string   host_;
    uint16_t      port_;
    MessageHub&   hub_;
    codec::Format outputFormat_;
    bool          keepalive_;

    static constexpr auto kConnectTimeout    = std::chrono::milliseconds{15'000};
    static constexpr auto kReconnectDelay    = std::chrono::milliseconds{10'000};
    static constexpr auto kHeartbeatInterval = std::chrono::milliseconds{60'000};
    static constexpr auto kIdleTimeout       = std::chrono::milliseconds{30'000};
};
