#pragma once

// Ferriot - CoAP Transport Layer
// Abstraction over libcoap with DTLS support

#include "../types.hpp"
#include "../result.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lwm2m::transport {

// CoAP methods
enum class CoapMethod {
    Get,
    Post,
    Put,
    Delete,
};

// CoAP response codes
enum class CoapCode : uint8_t {
    // Success 2.xx
    Created = 65,   // 2.01
    Deleted = 66,   // 2.02
    Valid = 67,     // 2.03
    Changed = 68,   // 2.04
    Content = 69,   // 2.05

    // Client Error 4.xx
    BadRequest = 128,        // 4.00
    Unauthorized = 129,      // 4.01
    BadOption = 130,         // 4.02
    Forbidden = 131,         // 4.03
    NotFound = 132,          // 4.04
    MethodNotAllowed = 133,  // 4.05
    NotAcceptable = 134,     // 4.06

    // Server Error 5.xx
    InternalServerError = 160,  // 5.00
    NotImplemented = 161,       // 5.01
    BadGateway = 162,           // 5.02
    ServiceUnavailable = 163,   // 5.03
    GatewayTimeout = 164,       // 5.04
};

// Content formats (CoAP option 12)
enum class ContentFormat : uint16_t {
    TextPlain = 0,
    LinkFormat = 40,
    Xml = 41,
    OctetStream = 42,
    Json = 50,
    Cbor = 60,
    // LWM2M specific
    TlvLwm2m = 11542,
    JsonLwm2m = 11543,
    SenmlJson = 110,
    SenmlCbor = 112,
};

// CoAP request
struct CoapRequest {
    CoapMethod method;
    std::string uri_path;
    std::vector<std::pair<uint16_t, std::vector<uint8_t>>> options;
    std::vector<uint8_t> payload;
    ContentFormat content_format = ContentFormat::TlvLwm2m;
};

// CoAP response
struct CoapResponse {
    CoapCode code;
    ContentFormat content_format = ContentFormat::TlvLwm2m;  // Default to TLV
    std::vector<std::pair<uint16_t, std::vector<uint8_t>>> options;
    std::vector<uint8_t> payload;

    [[nodiscard]] bool is_success() const noexcept {
        auto c = static_cast<uint8_t>(code);
        return c >= 64 && c < 128;  // 2.xx codes
    }

    // Extract Location-Path from options (CoAP option 8)
    // Used to get registration location after successful registration
    [[nodiscard]] std::string get_location_path() const {
        std::string location;
        for (const auto& [option_num, value] : options) {
            if (option_num == 8) {  // COAP_OPTION_LOCATION_PATH
                if (!location.empty()) {
                    location += "/";
                }
                location += std::string(
                    reinterpret_cast<const char*>(value.data()),
                    value.size()
                );
            }
        }
        return location.empty() ? location : "/" + location;
    }
};

// DTLS-PSK credentials
struct PskCredentials {
    std::string identity;
    std::vector<uint8_t> key;
};

// DTLS-RPK credentials (Raw Public Key)
struct RpkCredentials {
    std::vector<uint8_t> public_key;
    std::vector<uint8_t> private_key;
    std::vector<uint8_t> server_public_key;  // Optional, for verification
};

// Security mode
enum class SecurityMode {
    NoSec,
    Psk,
    Rpk,
    Certificate,
};

// Connection configuration
struct ConnectionConfig {
    std::string server_uri;  // coap:// or coaps://
    uint16_t port = 5683;
    SecurityMode security_mode = SecurityMode::NoSec;

    // DTLS credentials (one of these based on security_mode)
    std::optional<PskCredentials> psk;
    std::optional<RpkCredentials> rpk;

    // Timeouts
    std::chrono::seconds ack_timeout{2};
    std::chrono::seconds exchange_lifetime{247};
    uint8_t max_retransmit{4};
};

// Request handler callback (for server-initiated requests)
using RequestHandler = std::function<CoapResponse(const CoapRequest&)>;

// CoAP client interface
class CoapClient {
public:
    virtual ~CoapClient() = default;

    // Connection management
    [[nodiscard]] virtual Result<void> connect(const ConnectionConfig& config) = 0;
    virtual void disconnect() = 0;
    [[nodiscard]] virtual bool is_connected() const noexcept = 0;

    // Send request and wait for response
    [[nodiscard]] virtual Result<CoapResponse> send(
        const CoapRequest& request,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}
    ) = 0;

    // Register handler for server-initiated requests (Read, Write, Execute)
    virtual void set_request_handler(RequestHandler handler) = 0;

    // Process pending I/O (call periodically in event loop)
    virtual void poll(std::chrono::milliseconds timeout = std::chrono::milliseconds{100}) = 0;

    // Factory method
    [[nodiscard]] static std::unique_ptr<CoapClient> create();
};

// Helper functions
[[nodiscard]] std::string_view coap_method_to_string(CoapMethod method) noexcept;
[[nodiscard]] std::string_view coap_code_to_string(CoapCode code) noexcept;

} // namespace lwm2m::transport
