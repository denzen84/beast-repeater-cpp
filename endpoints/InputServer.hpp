#pragma once

#include "core/MessageHub.hpp"
#include "endpoints/Endpoint.hpp"
#include "input/InputParser.hpp"
#include "net/Socket.hpp"

#include <chrono>
#include <string>

class InputServer final : public Endpoint {
public:
    InputServer(uint16_t port, MessageHub& hub,
                std::string bindAddr = "0.0.0.0");
    ~InputServer() override { stop(); }

protected:
    void run(std::stop_token st) override;

private:
    void handleClient(net::Socket sock, std::string peer, std::stop_token st);

    uint16_t    port_;
    MessageHub& hub_;
    std::string bindAddr_;

    static constexpr auto kAcceptPoll  = std::chrono::milliseconds{500};
    static constexpr auto kReadTimeout = std::chrono::milliseconds{60'000};
};
