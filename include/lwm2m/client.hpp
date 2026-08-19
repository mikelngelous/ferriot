#pragma once

// Ferriot - Main Client Class
// High-level LWM2M client API

#include "types.hpp"
#include "result.hpp"
#include "object.hpp"
#include "transport/coap_client.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lwm2m {

// Forward declarations
namespace objects {
    class SecurityObject;
    class ServerObject;
    class DeviceObject;
    class ConnectivityObject;
    class FirmwareUpdateObject;
}

// Client state
enum class ClientState {
    Idle,
    Registering,
    Registered,
    Updating,
    Deregistering,
    BootstrapPending,
    Bootstrapping,
    Error,
};

// Registration info
struct Registration {
    std::string location;
    uint16_t short_server_id;
    std::chrono::system_clock::time_point registered_at;
    std::chrono::system_clock::time_point expires_at;
};

// Client configuration
struct ClientConfig {
    std::string endpoint_name;
    std::string lifetime = "86400";  // Default 24 hours
    std::string binding_mode = "U";  // UDP
    bool queue_mode = false;

    // Event loop configuration
    std::chrono::milliseconds poll_interval{100};

    // Auto-update before expiration (fraction of lifetime)
    double update_trigger_threshold = 0.8;
};

// Auto-reconnection configuration
struct ReconnectionConfig {
    bool enabled = true;
    uint16_t max_retries = 0;                     // 0 = infinite retries
    std::chrono::seconds initial_backoff{2};
    std::chrono::seconds max_backoff{300};        // 5 minutes
    double backoff_multiplier = 2.0;
};

// Event callbacks
struct ClientCallbacks {
    std::function<void(ClientState old_state, ClientState new_state)> on_state_change;
    std::function<void(uint16_t short_server_id, const Registration&)> on_registered;
    std::function<void(uint16_t short_server_id)> on_deregistered;
    std::function<void(ErrorCode code, std::string_view message)> on_error;

    // Reconnection callbacks
    std::function<void(uint16_t short_server_id)> on_connection_lost;
    std::function<void(uint16_t short_server_id, uint16_t attempt)> on_reconnecting;
    std::function<void(uint16_t short_server_id)> on_reconnected;
};

// LWM2M Client
class Client {
public:
    explicit Client(ClientConfig config);
    ~Client();

    // Non-copyable, non-movable (due to atomic members)
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(Client&&) = delete;

    // Object management
    void add_object(std::shared_ptr<Object> object);
    [[nodiscard]] Object* get_object(ObjectId oid);
    [[nodiscard]] const Object* get_object(ObjectId oid) const;

    // Convenience accessors for mandatory objects
    [[nodiscard]] objects::SecurityObject& security();
    [[nodiscard]] objects::ServerObject& server();
    [[nodiscard]] objects::DeviceObject& device();
    [[nodiscard]] objects::ConnectivityObject& connectivity();
    [[nodiscard]] objects::FirmwareUpdateObject& firmware_update();

    // Lifecycle
    [[nodiscard]] Result<void> start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept;

    // Registration management
    [[nodiscard]] Result<void> register_with_server(uint16_t short_server_id);
    [[nodiscard]] Result<void> update_registration(uint16_t short_server_id);
    [[nodiscard]] Result<void> deregister(uint16_t short_server_id);

    // Manual poll (if not using internal thread)
    void poll();

    // State
    [[nodiscard]] ClientState state() const noexcept;
    [[nodiscard]] std::string_view state_string() const noexcept;

    // Registration info
    [[nodiscard]] std::optional<Registration> get_registration(uint16_t short_server_id) const;
    [[nodiscard]] std::vector<uint16_t> registered_servers() const;

    // Event callbacks
    void set_callbacks(ClientCallbacks callbacks);

    // Configuration
    [[nodiscard]] const ClientConfig& config() const noexcept { return config_; }

    // Reconnection configuration
    void set_reconnection_config(ReconnectionConfig config) { reconnect_config_ = std::move(config); }
    [[nodiscard]] const ReconnectionConfig& reconnection_config() const noexcept { return reconnect_config_; }

private:
    // Internal methods
    void run_event_loop();
    [[nodiscard]] transport::CoapResponse handle_incoming_request(const transport::CoapRequest& request);
    transport::CoapResponse process_read(const ObjectPath& path);
    transport::CoapResponse process_write(const ObjectPath& path, const std::vector<uint8_t>& payload,
                                          transport::ContentFormat content_format);
    transport::CoapResponse process_execute(const ObjectPath& path, const std::vector<uint8_t>& payload);
    transport::CoapResponse process_discover(const ObjectPath& path);
    transport::CoapResponse process_delete(const ObjectPath& path);

    // Helper methods
    [[nodiscard]] static transport::CoapCode error_to_coap_code(ErrorCode code) noexcept;
    [[nodiscard]] std::vector<uint8_t> encode_instance_tlv(Object* obj, InstanceId iid) const;

    void set_state(ClientState new_state);
    void check_registration_updates();
    [[nodiscard]] std::string build_registration_payload() const;

    ClientConfig config_;
    ClientCallbacks callbacks_;

    std::unordered_map<uint16_t, std::shared_ptr<Object>> objects_;
    std::unordered_map<uint16_t, std::unique_ptr<transport::CoapClient>> connections_;
    std::unordered_map<uint16_t, Registration> registrations_;

    // Mandatory object pointers (for quick access)
    std::shared_ptr<objects::SecurityObject> security_;
    std::shared_ptr<objects::ServerObject> server_;
    std::shared_ptr<objects::DeviceObject> device_;
    std::shared_ptr<objects::ConnectivityObject> connectivity_;
    std::shared_ptr<objects::FirmwareUpdateObject> firmware_update_;

    std::atomic<ClientState> state_{ClientState::Idle};
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> event_thread_;

    // Serializes start()/stop(); mtx_ guards data, this guards the thread
    std::mutex lifecycle_mtx_;

    // Thread safety for shared containers
    // Using recursive_mutex to allow nested calls from within locked methods
    mutable std::recursive_mutex mtx_;

    // Reconnection state
    ReconnectionConfig reconnect_config_;
    std::atomic<bool> reconnecting_{false};
    uint16_t reconnect_attempts_{0};
    uint16_t reconnect_ssid_{0};  // Server ID being reconnected
    std::chrono::steady_clock::time_point next_reconnect_time_;

    // Reconnection methods
    void check_connection_health();
    void handle_connection_lost(uint16_t ssid);
    void try_reconnect(uint16_t ssid);
    [[nodiscard]] std::chrono::seconds calculate_backoff() const;
};

// Utility functions
[[nodiscard]] std::string_view client_state_to_string(ClientState state) noexcept;

} // namespace lwm2m
