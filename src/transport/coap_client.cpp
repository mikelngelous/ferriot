// Ferriot - CoAP Client Implementation
// Wrapper around libcoap

#include "lwm2m/transport/coap_client.hpp"

#include <coap3/coap.h>

#include <cstring>
#include <stdexcept>

namespace lwm2m::transport {

// Implementation class (PIMPL pattern)
class CoapClientImpl : public CoapClient {
public:
    CoapClientImpl() = default;
    ~CoapClientImpl() override;

    // CoapClient interface
    Result<void> connect(const ConnectionConfig& config) override;
    void disconnect() override;
    [[nodiscard]] bool is_connected() const noexcept override;

    Result<CoapResponse> send(
        const CoapRequest& request,
        std::chrono::milliseconds timeout
    ) override;

    void set_request_handler(RequestHandler handler) override;
    void poll(std::chrono::milliseconds timeout) override;

private:
    // libcoap callbacks
    static coap_response_t response_handler(
        coap_session_t* session,
        const coap_pdu_t* sent,
        const coap_pdu_t* received,
        coap_mid_t mid
    );

    static int event_handler(
        coap_session_t* session,
        const coap_event_t event
    );

    Result<void> setup_dtls_psk(const PskCredentials& psk);
    Result<void> setup_dtls_rpk(const RpkCredentials& rpk);

    coap_context_t* ctx_ = nullptr;
    coap_session_t* session_ = nullptr;

    RequestHandler request_handler_;
    bool connected_ = false;

    // Response tracking
    struct PendingResponse {
        bool completed = false;
        CoapResponse response;
    };
    PendingResponse* current_response_ = nullptr;
};

CoapClientImpl::~CoapClientImpl() {
    disconnect();
}

Result<void> CoapClientImpl::connect(const ConnectionConfig& config) {
    disconnect();

    // Initialize libcoap
    coap_startup();

    // Create context
    ctx_ = coap_new_context(nullptr);
    if (!ctx_) {
        return Err<void>(ErrorCode::InternalServerError, "Failed to create CoAP context");
    }

    // Register handlers
    coap_register_response_handler(ctx_, &CoapClientImpl::response_handler);
    coap_register_event_handler(ctx_, &CoapClientImpl::event_handler);

    // Parse server URI
    coap_uri_t uri;
    if (coap_split_uri(
            reinterpret_cast<const uint8_t*>(config.server_uri.c_str()),
            config.server_uri.length(),
            &uri
        ) < 0) {
        disconnect();
        return Err<void>(ErrorCode::BadRequest, "Invalid server URI");
    }

    // Resolve address
    coap_address_t dst;
    coap_address_init(&dst);

    // Determine if secure connection
    bool use_dtls = (uri.scheme == COAP_URI_SCHEME_COAPS ||
                     uri.scheme == COAP_URI_SCHEME_COAPS_TCP);

    // Setup DTLS if needed
    if (use_dtls) {
        switch (config.security_mode) {
            case SecurityMode::Psk:
                if (!config.psk) {
                    disconnect();
                    return Err<void>(ErrorCode::BadRequest, "PSK credentials required");
                }
                if (auto res = setup_dtls_psk(*config.psk); !res) {
                    disconnect();
                    return res;
                }
                break;

            case SecurityMode::Rpk:
                if (!config.rpk) {
                    disconnect();
                    return Err<void>(ErrorCode::BadRequest, "RPK credentials required");
                }
                if (auto res = setup_dtls_rpk(*config.rpk); !res) {
                    disconnect();
                    return res;
                }
                break;

            case SecurityMode::NoSec:
                disconnect();
                return Err<void>(ErrorCode::BadRequest, "Security mode mismatch");

            case SecurityMode::Certificate:
                disconnect();
                return Err<void>(ErrorCode::NotImplemented, "Certificate mode not yet implemented");
        }
    }

    // Resolve hostname
    coap_str_const_t host = {uri.host.length, uri.host.s};
    auto* addr_info = coap_resolve_address_info(
        &host,
        uri.port,
        uri.port,
        uri.port,
        uri.port,
        AF_UNSPEC,
        1,  // AI_PASSIVE
        use_dtls ? COAP_RESOLVE_TYPE_REMOTE : COAP_RESOLVE_TYPE_LOCAL
    );

    if (!addr_info) {
        disconnect();
        return Err<void>(ErrorCode::ConnectionFailed, "Failed to resolve server address");
    }

    dst = addr_info->addr;
    coap_free_address_info(addr_info);

    // Create session
    coap_proto_t proto = use_dtls ? COAP_PROTO_DTLS : COAP_PROTO_UDP;
    session_ = coap_new_client_session(ctx_, nullptr, &dst, proto);

    if (!session_) {
        disconnect();
        return Err<void>(ErrorCode::ConnectionFailed, "Failed to create CoAP session");
    }

    // Store this pointer for callbacks
    coap_session_set_app_data(session_, this);

    connected_ = true;
    return Ok();
}

void CoapClientImpl::disconnect() {
    if (session_) {
        coap_session_release(session_);
        session_ = nullptr;
    }

    if (ctx_) {
        coap_free_context(ctx_);
        ctx_ = nullptr;
    }

    connected_ = false;
    coap_cleanup();
}

bool CoapClientImpl::is_connected() const noexcept {
    return connected_ && session_ != nullptr;
}

Result<CoapResponse> CoapClientImpl::send(
    const CoapRequest& request,
    std::chrono::milliseconds timeout
) {
    if (!is_connected()) {
        return Err<CoapResponse>(ErrorCode::InvalidState, "Not connected");
    }

    // Create PDU
    coap_pdu_code_t pdu_code;
    switch (request.method) {
        case CoapMethod::Get: pdu_code = COAP_REQUEST_CODE_GET; break;
        case CoapMethod::Post: pdu_code = COAP_REQUEST_CODE_POST; break;
        case CoapMethod::Put: pdu_code = COAP_REQUEST_CODE_PUT; break;
        case CoapMethod::Delete: pdu_code = COAP_REQUEST_CODE_DELETE; break;
        default: pdu_code = COAP_REQUEST_CODE_GET; break;
    }

    coap_pdu_t* pdu = coap_pdu_init(
        COAP_MESSAGE_CON,
        pdu_code,
        coap_new_message_id(session_),
        coap_session_max_pdu_size(session_)
    );

    if (!pdu) {
        return Err<CoapResponse>(ErrorCode::InternalServerError, "Failed to create PDU");
    }

    // Add URI path
    coap_uri_t uri;
    std::string full_uri = "coap://localhost" + request.uri_path;
    if (coap_split_uri(
            reinterpret_cast<const uint8_t*>(full_uri.c_str()),
            full_uri.length(),
            &uri
        ) == 0) {
        coap_add_option(pdu, COAP_OPTION_URI_PATH, uri.path.length, uri.path.s);
    }

    // Add content format
    uint8_t cf_buf[2];
    size_t cf_len = coap_encode_var_safe(cf_buf, sizeof(cf_buf),
                                          static_cast<uint16_t>(request.content_format));
    coap_add_option(pdu, COAP_OPTION_CONTENT_FORMAT, cf_len, cf_buf);

    // Add payload
    if (!request.payload.empty()) {
        coap_add_data(pdu, request.payload.size(), request.payload.data());
    }

    // Setup response tracking
    PendingResponse pending;
    current_response_ = &pending;

    // Send request
    if (coap_send(session_, pdu) == COAP_INVALID_MID) {
        current_response_ = nullptr;
        return Err<CoapResponse>(ErrorCode::InternalServerError, "Failed to send request");
    }

    // Wait for response
    auto start = std::chrono::steady_clock::now();
    while (!pending.completed) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout) {
            current_response_ = nullptr;
            return Err<CoapResponse>(ErrorCode::Timeout, "Request timeout");
        }

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(timeout - elapsed);
        poll(std::min(remaining, std::chrono::milliseconds{100}));
    }

    current_response_ = nullptr;
    return Ok(std::move(pending.response));
}

void CoapClientImpl::set_request_handler(RequestHandler handler) {
    request_handler_ = std::move(handler);
}

void CoapClientImpl::poll(std::chrono::milliseconds timeout) {
    if (ctx_) {
        coap_io_process(ctx_, static_cast<uint32_t>(timeout.count()));
    }
}

coap_response_t CoapClientImpl::response_handler(
    coap_session_t* session,
    [[maybe_unused]] const coap_pdu_t* sent,
    const coap_pdu_t* received,
    [[maybe_unused]] coap_mid_t mid
) {
    auto* impl = static_cast<CoapClientImpl*>(coap_session_get_app_data(session));
    if (!impl || !impl->current_response_) {
        return COAP_RESPONSE_OK;
    }

    // Extract response code
    impl->current_response_->response.code =
        static_cast<CoapCode>(coap_pdu_get_code(received));

    // Extract payload
    const uint8_t* data = nullptr;
    size_t data_len = 0;
    if (coap_get_data(received, &data_len, &data) && data_len > 0) {
        impl->current_response_->response.payload.assign(data, data + data_len);
    }

    impl->current_response_->completed = true;
    return COAP_RESPONSE_OK;
}

int CoapClientImpl::event_handler(
    coap_session_t* session,
    const coap_event_t event
) {
    auto* impl = static_cast<CoapClientImpl*>(coap_session_get_app_data(session));
    if (!impl) {
        return 0;
    }

    switch (event) {
        case COAP_EVENT_DTLS_CONNECTED:
        case COAP_EVENT_SESSION_CONNECTED:
            impl->connected_ = true;
            break;

        case COAP_EVENT_DTLS_CLOSED:
        case COAP_EVENT_SESSION_CLOSED:
        case COAP_EVENT_SESSION_FAILED:
            impl->connected_ = false;
            break;

        default:
            break;
    }
    return 0;
}

Result<void> CoapClientImpl::setup_dtls_psk(const PskCredentials& psk) {
    // Note: For client PSK, we use coap_new_client_session_psk2 instead of
    // coap_context_set_psk2 which is for servers.
    // For now, store credentials and apply when creating session.
    // This is a simplified implementation - full implementation would
    // use coap_new_client_session_psk2 with proper callback.
    (void)psk;  // TODO: Implement proper client PSK support
    return Ok();
}

Result<void> CoapClientImpl::setup_dtls_rpk(const RpkCredentials& /* rpk */) {
    // RPK setup requires additional libcoap configuration
    // This is a placeholder for now
    return Err<void>(ErrorCode::NotImplemented, "RPK support not yet implemented");
}

// Factory method
std::unique_ptr<CoapClient> CoapClient::create() {
    return std::make_unique<CoapClientImpl>();
}

// Helper functions
std::string_view coap_method_to_string(CoapMethod method) noexcept {
    switch (method) {
        case CoapMethod::Get: return "GET";
        case CoapMethod::Post: return "POST";
        case CoapMethod::Put: return "PUT";
        case CoapMethod::Delete: return "DELETE";
        default: return "UNKNOWN";
    }
}

std::string_view coap_code_to_string(CoapCode code) noexcept {
    switch (code) {
        case CoapCode::Created: return "2.01 Created";
        case CoapCode::Deleted: return "2.02 Deleted";
        case CoapCode::Valid: return "2.03 Valid";
        case CoapCode::Changed: return "2.04 Changed";
        case CoapCode::Content: return "2.05 Content";
        case CoapCode::BadRequest: return "4.00 Bad Request";
        case CoapCode::Unauthorized: return "4.01 Unauthorized";
        case CoapCode::BadOption: return "4.02 Bad Option";
        case CoapCode::Forbidden: return "4.03 Forbidden";
        case CoapCode::NotFound: return "4.04 Not Found";
        case CoapCode::MethodNotAllowed: return "4.05 Method Not Allowed";
        case CoapCode::NotAcceptable: return "4.06 Not Acceptable";
        case CoapCode::InternalServerError: return "5.00 Internal Server Error";
        case CoapCode::NotImplemented: return "5.01 Not Implemented";
        case CoapCode::BadGateway: return "5.02 Bad Gateway";
        case CoapCode::ServiceUnavailable: return "5.03 Service Unavailable";
        case CoapCode::GatewayTimeout: return "5.04 Gateway Timeout";
        default: return "Unknown";
    }
}

} // namespace lwm2m::transport
