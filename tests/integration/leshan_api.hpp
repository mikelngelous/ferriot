#pragma once

// Leshan REST API Client for Integration Tests
// Uses libcurl + jsoncpp

#include <string>
#include <optional>
#include <vector>
#include <chrono>

#include <json/json.h>

namespace lwm2m::test {

// Client registration info from Leshan
struct LeshanClient {
    std::string endpoint;
    std::string registration_id;
    std::string address;
    std::string lwm2m_version;
    uint32_t lifetime;
    std::string binding_mode;
    std::chrono::system_clock::time_point registration_date;
    std::chrono::system_clock::time_point last_update;
    std::vector<std::string> object_links;
};

// Resource value from Leshan read operation
struct LeshanResourceValue {
    enum class Type { String, Integer, Float, Boolean, Opaque, Time, ObjectLink };

    Type type;
    std::string string_value;
    int64_t int_value = 0;
    double float_value = 0.0;
    bool bool_value = false;
    std::vector<uint8_t> opaque_value;
};

// Leshan REST API Client
class LeshanApi {
public:
    explicit LeshanApi(std::string base_url = "http://localhost:8080");
    ~LeshanApi();

    // Non-copyable
    LeshanApi(const LeshanApi&) = delete;
    LeshanApi& operator=(const LeshanApi&) = delete;

    // Connection
    [[nodiscard]] bool is_server_available() const;

    // Client management
    [[nodiscard]] std::vector<LeshanClient> get_clients() const;
    [[nodiscard]] std::optional<LeshanClient> get_client(const std::string& endpoint) const;
    [[nodiscard]] bool client_exists(const std::string& endpoint) const;
    [[nodiscard]] bool wait_for_client(const std::string& endpoint,
                                        std::chrono::seconds timeout = std::chrono::seconds{30}) const;
    [[nodiscard]] bool wait_for_client_gone(const std::string& endpoint,
                                             std::chrono::seconds timeout = std::chrono::seconds{30}) const;

    // Resource operations (via Leshan server -> client)
    [[nodiscard]] std::optional<LeshanResourceValue> read_resource(
        const std::string& endpoint,
        uint16_t object_id,
        uint16_t instance_id,
        uint16_t resource_id
    ) const;

    [[nodiscard]] bool write_resource(
        const std::string& endpoint,
        uint16_t object_id,
        uint16_t instance_id,
        uint16_t resource_id,
        const LeshanResourceValue& value
    ) const;

    [[nodiscard]] bool execute_resource(
        const std::string& endpoint,
        uint16_t object_id,
        uint16_t instance_id,
        uint16_t resource_id,
        const std::string& arguments = ""
    ) const;

    // Observe operations
    [[nodiscard]] bool observe(
        const std::string& endpoint,
        uint16_t object_id,
        uint16_t instance_id,
        uint16_t resource_id
    ) const;

    [[nodiscard]] bool cancel_observe(
        const std::string& endpoint,
        uint16_t object_id,
        uint16_t instance_id,
        uint16_t resource_id
    ) const;

    // Delete client registration
    [[nodiscard]] bool delete_client(const std::string& endpoint) const;

    // Get last error message
    [[nodiscard]] const std::string& last_error() const { return last_error_; }

private:
    // HTTP operations
    [[nodiscard]] std::optional<std::string> http_get(const std::string& path) const;
    [[nodiscard]] std::optional<std::string> http_post(const std::string& path,
                                                        const std::string& body = "") const;
    [[nodiscard]] std::optional<std::string> http_put(const std::string& path,
                                                       const std::string& body) const;
    [[nodiscard]] bool http_delete(const std::string& path) const;

    // JSON parsing
    [[nodiscard]] std::optional<Json::Value> parse_json(const std::string& json_str) const;
    [[nodiscard]] LeshanClient parse_client(const Json::Value& json) const;
    [[nodiscard]] LeshanResourceValue parse_resource_value(const Json::Value& json) const;

    std::string base_url_;
    mutable std::string last_error_;
};

} // namespace lwm2m::test
