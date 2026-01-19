// Ferriot
// Main client application

#include "lwm2m/client.hpp"
#include "lwm2m/objects/security.hpp"
#include "lwm2m/objects/server.hpp"
#include "lwm2m/objects/device.hpp"

#include <csignal>
#include <iostream>
#include <atomic>

namespace {
    std::atomic<bool> running{true};

    void signal_handler(int /* sig */) {
        running.store(false);
    }

    // Helper functions to create configurations (C++17 compatible)
    lwm2m::ClientConfig make_client_config(
        const std::string& endpoint,
        const std::string& lifetime = "300",
        const std::string& binding = "U"
    ) {
        lwm2m::ClientConfig config;
        config.endpoint_name = endpoint;
        config.lifetime = lifetime;
        config.binding_mode = binding;
        return config;
    }

    lwm2m::objects::DeviceConfig make_device_config(
        const std::string& manufacturer,
        const std::string& model,
        const std::string& serial,
        const std::string& fw_version,
        const std::string& device_type
    ) {
        lwm2m::objects::DeviceConfig config;
        config.manufacturer = manufacturer;
        config.model_number = model;
        config.serial_number = serial;
        config.firmware_version = fw_version;
        config.device_type = device_type;
        return config;
    }

    lwm2m::objects::SecurityInstance make_security_instance(
        const std::string& uri,
        uint16_t ssid,
        lwm2m::objects::SecurityModeValue mode = lwm2m::objects::SecurityModeValue::NoSec
    ) {
        lwm2m::objects::SecurityInstance inst;
        inst.server_uri = uri;
        inst.bootstrap_server = false;
        inst.security_mode = mode;
        inst.short_server_id = ssid;
        return inst;
    }

    lwm2m::objects::ServerInstance make_server_instance(
        uint16_t ssid,
        uint32_t lifetime,
        const std::string& binding = "U"
    ) {
        lwm2m::objects::ServerInstance inst;
        inst.short_server_id = ssid;
        inst.lifetime = lifetime;
        inst.binding = binding;
        return inst;
    }
}

int main(int argc, char* argv[]) {
    std::string server_uri = "coap://localhost:5683";
    std::string endpoint_name = "cpp-client-1";
    std::string security_mode = "nosec";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-u" || arg == "--uri") && i + 1 < argc) {
            server_uri = argv[++i];
        } else if ((arg == "-e" || arg == "--endpoint") && i + 1 < argc) {
            endpoint_name = argv[++i];
        } else if ((arg == "-s" || arg == "--security") && i + 1 < argc) {
            security_mode = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -u, --uri URI        Server URI (default: coap://localhost:5683)\n"
                      << "  -e, --endpoint NAME  Endpoint name (default: cpp-client-1)\n"
                      << "  -s, --security MODE  Security mode: nosec, psk (default: nosec)\n"
                      << "  -h, --help           Show this help\n";
            return 0;
        }
    }

    std::cout << "Ferriot - Minimal Example\n"
              << "==================================\n"
              << "Server URI: " << server_uri << "\n"
              << "Endpoint: " << endpoint_name << "\n"
              << "Security: " << security_mode << "\n\n";

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Create client with configuration (short lifetime for faster reconnection)
    auto config = make_client_config(endpoint_name, "15", "U");
    lwm2m::Client client(config);

    auto device_config = make_device_config(
        "Ferriot",
        "Ferriot",
        "001",
        "0.1.0",
        "TCU"
    );
    client.device() = lwm2m::objects::DeviceObject(device_config);

    auto sec_config = make_security_instance(
        server_uri,
        1,
        lwm2m::objects::SecurityModeValue::NoSec
    );
    client.security().add_server(sec_config);

    // Add server instance (short lifetime for faster reconnection)
    auto srv_config = make_server_instance(1, 15, "U");
    client.server().add_server(srv_config);

    lwm2m::ClientCallbacks callbacks;
    callbacks.on_state_change = [](lwm2m::ClientState old_state, lwm2m::ClientState new_state) {
        std::cout << "State: " << lwm2m::client_state_to_string(old_state)
                  << " -> " << lwm2m::client_state_to_string(new_state) << "\n";
    };
    callbacks.on_registered = [](uint16_t ssid, const lwm2m::Registration& reg) {
        std::cout << "Registered with server " << ssid
                  << " at " << reg.location << "\n";
    };
    callbacks.on_deregistered = [](uint16_t ssid) {
        std::cout << "Deregistered from server " << ssid << "\n";
    };
    callbacks.on_error = [](lwm2m::ErrorCode code, std::string_view message) {
        std::cerr << "Error " << static_cast<int>(code) << ": " << message << "\n";
    };

    // Reconnection callbacks
    callbacks.on_connection_lost = [](uint16_t ssid) {
        std::cerr << "[WARN] Connection lost to server " << ssid << "\n";
    };
    callbacks.on_reconnecting = [](uint16_t ssid, uint16_t attempt) {
        std::cout << "[INFO] Reconnecting to server " << ssid
                  << "... attempt " << attempt << "\n";
    };
    callbacks.on_reconnected = [](uint16_t ssid) {
        std::cout << "[INFO] Reconnected to server " << ssid << "\n";
    };

    client.set_callbacks(callbacks);

    if (auto result = client.start(); !result) {
        std::cerr << "Failed to start client: " << result.error().message() << "\n";
        return 1;
    }

    if (auto result = client.register_with_server(1); !result) {
        std::cerr << "Failed to register: " << result.error().message() << "\n";
        return 1;
    }

    std::cout << "Client running. Press Ctrl+C to stop.\n";

    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }

    std::cout << "\nShutting down...\n";
    client.stop();

    return 0;
}
