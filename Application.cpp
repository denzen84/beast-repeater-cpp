#include "Application.hpp"
#include "compat/format.hpp"  // ← было: #include <format>

#include "endpoints/InputConnector.hpp"
#include "endpoints/InputServer.hpp"
#include "endpoints/OutputConnector.hpp"
#include "endpoints/OutputServer.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
    std::atomic<bool> g_shutdown{false};
    void sigHandler(int) { g_shutdown.store(true, std::memory_order_relaxed); }
} // namespace

int Application::run(int argc, char* argv[]) {
    std::vector<EndpointSpec> specs;
    bool keepalive = false;

    try {
        if (!parseArgs(argc, argv, specs, keepalive)) return 0;
        if (specs.empty()) {
            std::cerr << "No endpoints specified. Use --help for usage.\n";
            return 1;
        }
        buildEndpoints(specs, keepalive);
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << '\n';
        return 1;
    }

    ::signal(SIGINT,  sigHandler);
    ::signal(SIGTERM, sigHandler);
    ::signal(SIGPIPE, SIG_IGN);

    for (auto& ep : endpoints_) ep->start();
    std::cerr << "beast-repeater running. Press Ctrl+C to stop.\n";

    while (!g_shutdown.load(std::memory_order_relaxed))
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

    std::cerr << "\nShutting down...\n";
    for (auto it = endpoints_.rbegin(); it != endpoints_.rend(); ++it)
        (*it)->stop();

    const auto st = hub_.stats();
    std::cerr << std::format("Stats: published={} dropped={} subscribers={}\n",
                              st.published, st.totalDropped, st.subscribers);
    std::cerr << "Done.\n";
    return 0;
}

bool Application::parseArgs(int argc, char* argv[],
                              std::vector<EndpointSpec>& specs, bool& keepalive) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);

        if (arg == "-h" || arg == "--help") { printHelp(argv[0]); return false; }
        if (arg == "--keep")                { keepalive = true; continue; }

        const bool hasNext = (i + 1 < argc);

        if (arg == "--inConnect" && hasNext) {
            auto [h, p] = parseHostPort(argv[++i]);
            specs.push_back({ EndpointSpec::Kind::InConnect, std::move(h), p });

        } else if (arg == "--inServer" && hasNext) {
            specs.push_back({ EndpointSpec::Kind::InServer, {}, parsePort(argv[++i]) });

        } else if (arg == "--outConnect" && hasNext) {
            auto [fmt, h, p] = parseOutConnectSpec(argv[++i]);
            specs.push_back({ EndpointSpec::Kind::OutConnect, std::move(h), p, fmt });

        } else if (arg == "--outServer" && hasNext) {
            auto [fmt, p] = parseOutServerSpec(argv[++i]);
            specs.push_back({ EndpointSpec::Kind::OutServer, {}, p, fmt });

        } else {
            std::cerr << std::format("Unknown or incomplete option: '{}'\n\n", arg);
            printHelp(argv[0]);
            return false;
        }
    }
    return true;
}

void Application::buildEndpoints(const std::vector<EndpointSpec>& specs,
                                   bool keepalive) {
    for (const auto& s : specs) {
        switch (s.kind) {
        case EndpointSpec::Kind::InConnect:
            endpoints_.push_back(
                std::make_unique<InputConnector>(s.host, s.port, hub_));
            break;
        case EndpointSpec::Kind::InServer:
            endpoints_.push_back(
                std::make_unique<InputServer>(s.port, hub_));
            break;
        case EndpointSpec::Kind::OutConnect:
            endpoints_.push_back(
                std::make_unique<OutputConnector>(s.host, s.port, hub_,
                                                  s.outputFormat, keepalive));
            break;
        case EndpointSpec::Kind::OutServer:
            endpoints_.push_back(
                std::make_unique<OutputServer>(s.port, hub_,
                                               s.outputFormat, keepalive));
            break;
        }
    }
}

void Application::printHelp(const char* prog) {
    std::cout << std::format(
"Usage: {} [options...]\n"
"\n"
"Program options:\n"
"  -h [ --help ]             This help message\n"
"  --inConnect arg           Input connector.  Format: host:port\n"
"  --inServer  arg           Input server.     Format: port\n"
"  --outConnect arg          Output connector. Format: type:host:port\n"
"  --outServer  arg          Output server.    Format: type:port\n"
"  --keep                    Enable Beast keepalive message\n"
"                            (Beast output only, default: off)\n"
"\n"
"Output format types:  beast | avr | avrmlat\n"
"\n"
"Input format is auto-detected per connection:\n"
"  0x1A byte prefix  →  Beast Binary\n"
"  '*'  prefix       →  AVR Standard  (*HEXDATA;)\n"
"  '@'  prefix       →  AVR MLAT      (@TSHEXDATA;)\n"
"\n"
"Transcoding matrix (in → out):\n"
"  in \\ out   beast            avr              avrmlat\n"
"  beast    → copy             frame only       frame + timestamp\n"
"  avr      → ts=0, sig=0      copy             frame, ts=0\n"
"  avrmlat  → ts, sig=0        frame only       copy\n"
"\n"
"Notes:\n"
"  --keep sends Beast heartbeat every 60s on idle Beast outputs only.\n"
"  Beast type-4/5 config frames: forwarded in Beast mode,\n"
"  silently skipped for AVR outputs.\n"
"  All options may be repeated to create multiple endpoints.\n"
"\n"
"Examples:\n"
"  {} --inConnect 192.168.1.10:30005 \\\n"
"     --outServer beast:30005 --outServer avr:30002 --outServer avrmlat:30003 --keep\n"
"\n"
"  {} --inServer 30002 --outConnect beast:aggregator.local:10001\n",
        prog, prog, prog);
}

codec::Format Application::parseFormat(std::string_view s) {
    if (s == "beast")   return codec::Format::Beast;
    if (s == "avr")     return codec::Format::AvrStd;
    if (s == "avrmlat") return codec::Format::AvrMlat;
    throw std::runtime_error(
        std::format("Unknown format type: '{}'. Use: beast, avr, avrmlat", s));
}

std::tuple<codec::Format, std::string, uint16_t>
Application::parseOutConnectSpec(std::string_view s) {
    const auto firstColon = s.find(':');
    if (firstColon == std::string_view::npos)
        throw std::runtime_error(
            std::format("Invalid --outConnect '{}'. Expected type:host:port", s));

    auto fmt          = parseFormat(s.substr(0, firstColon));
    auto [host, port] = parseHostPort(s.substr(firstColon + 1));
    return { fmt, std::move(host), port };
}

std::pair<codec::Format, uint16_t>
Application::parseOutServerSpec(std::string_view s) {
    const auto colon = s.find(':');
    if (colon == std::string_view::npos)
        throw std::runtime_error(
            std::format("Invalid --outServer '{}'. Expected type:port", s));

    return { parseFormat(s.substr(0, colon)), parsePort(s.substr(colon + 1)) };
}

std::pair<std::string, uint16_t>
Application::parseHostPort(std::string_view s) {
    const auto colon = s.rfind(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 >= s.size())
        throw std::runtime_error(
            std::format("Invalid host:port spec '{}'", s));
    return { std::string(s.substr(0, colon)), parsePort(s.substr(colon + 1)) };
}

uint16_t Application::parsePort(std::string_view s) {
    uint32_t port = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), port);
    if (ec != std::errc{} || ptr != s.data() + s.size() || port == 0 || port > 65535)
        throw std::runtime_error(std::format("Invalid port '{}'", s));
    return static_cast<uint16_t>(port);
}
