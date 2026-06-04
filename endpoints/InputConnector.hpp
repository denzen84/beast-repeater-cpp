#pragma once

#include "core/MessageHub.hpp"
#include "endpoints/Endpoint.hpp"
#include "input/InputParser.hpp"
#include "net/Socket.hpp"

#include <chrono>
#include <string>

class InputConnector final : public Endpoint {
public:
    InputConnector(std::string host, uint16_t port, MessageHub& hub);
    ~InputConnector() override { stop(); }

protected:
    void run(std::stop_token st) override;

private:
    void handleConnection(net::Socket sock, std::stop_token st);

    std::string  host_;
    uint16_t     port_;
    MessageHub&  hub_;

    static constexpr auto kConnectTimeout = std::chrono::milliseconds{15'000};
    static constexpr auto kReconnectDelay = std::chrono::milliseconds{10'000};
    static constexpr auto kReadTimeout    = std::chrono::milliseconds{60'000};
};
