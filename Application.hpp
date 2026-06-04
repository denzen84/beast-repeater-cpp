#pragma once

#include "codec/Message.hpp"
#include "core/MessageHub.hpp"
#include "endpoints/Endpoint.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

class Application {
public:
    int run(int argc, char* argv[]);

private:
    struct EndpointSpec {
        enum class Kind { InConnect, InServer, OutConnect, OutServer } kind;
        std::string   host;
        uint16_t      port{};
        codec::Format outputFormat = codec::Format::Beast; // только для out-endpoint-ов
    };

    bool parseArgs(int argc, char* argv[],
                   std::vector<EndpointSpec>& specs,
                   bool& keepalive);

    void buildEndpoints(const std::vector<EndpointSpec>& specs, bool keepalive);

    static void printHelp(const char* prog);

    // "type:host:port" → {Format, host, port}
    static std::tuple<codec::Format, std::string, uint16_t>
        parseOutConnectSpec(std::string_view s);

    // "type:port" → {Format, port}
    static std::pair<codec::Format, uint16_t>
        parseOutServerSpec(std::string_view s);

    // "host:port" → {host, port}  (для --inConnect)
    static std::pair<std::string, uint16_t>
        parseHostPort(std::string_view s);

    static uint16_t      parsePort  (std::string_view s);
    static codec::Format parseFormat(std::string_view s);

    MessageHub                             hub_;
    std::vector<std::unique_ptr<Endpoint>> endpoints_;
};
