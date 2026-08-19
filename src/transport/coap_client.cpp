// Ferriot - CoAP Client Implementation
// Wrapper around libcoap

#include "lwm2m/transport/coap_client.hpp"

#include <coap3/coap.h>

#include <atomic>
#include <cstring>
#include <mutex>
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
    // Internal poll without locking (caller must hold io_mtx_)
    void poll_unlocked(std::chrono::milliseconds timeout);

    // Protects libcoap calls (not thread-safe). Recursive: connect() holds it
    // while calling disconnect().
    mutable std::recursive_mutex io_mtx_;

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

    // Handler for incoming requests from server (READ/WRITE/EXECUTE)
    static void incoming_request_handler(
        coap_resource_t* resource,
        coap_session_t* session,
        const coap_pdu_t* request,
        const coap_string_t* query,
        coap_pdu_t* response
    );

    Result<void> setup_dtls_psk(const PskCredentials& psk);
    Result<void> setup_dtls_rpk(const RpkCredentials& rpk);

    coap_context_t* ctx_ = nullptr;
    coap_session_t* session_ = nullptr;

    RequestHandler request_handler_;
    std::atomic<bool> connected_{false};

    // PSK credentials storage (must outlive session)
    std::string psk_identity_;
    std::vector<uint8_t> psk_key_;

    // Response tracking
    struct PendingResponse {
        bool completed = false;
        CoapResponse response;
        std::vector<uint8_t> token;
    };
    uint64_t next_token_ = 0;
    PendingResponse* current_response_ = nullptr;
};

CoapClientImpl::~CoapClientImpl() {
    disconnect();
}

// Static reference counter for libcoap initialization
namespace {
    std::atomic<int> coap_init_count{0};

    void init_libcoap() {
        if (coap_init_count.fetch_add(1) == 0) {
            coap_startup();
        }
    }

    void cleanup_libcoap() {
        if (coap_init_count.fetch_sub(1) == 1) {
            coap_cleanup();
        }
    }
}

Result<void> CoapClientImpl::connect(const ConnectionConfig& config) {
    std::lock_guard<std::recursive_mutex> lock(io_mtx_);
    disconnect();

    // Initialize libcoap (reference counted)
    init_libcoap();

    // Set log level to warnings only (debug was: COAP_LOG_DEBUG)
    coap_set_log_level(COAP_LOG_WARN);

    ctx_ = coap_new_context(nullptr);
    if (!ctx_) {
        return Err<void>(ErrorCode::InternalServerError, "Failed to create CoAP context");
    }

    // Register handlers for outgoing requests
    coap_register_response_handler(ctx_, &CoapClientImpl::response_handler);
    coap_register_event_handler(ctx_, &CoapClientImpl::event_handler);

    // Register "unknown" resource to handle all incoming requests from server
    // This is critical for LWM2M - the server sends READ/WRITE/EXECUTE to the client
    coap_resource_t* unknown_resource = coap_resource_unknown_init2(
        &CoapClientImpl::incoming_request_handler,
        0  // flags: no special flags
    );
    if (unknown_resource) {
        // Store this pointer directly on the resource for reliable access in handler
        coap_resource_set_userdata(unknown_resource, this);

        // IMPORTANT: coap_resource_unknown_init2 only sets PUT handler by default.
        // We need to register handlers for ALL methods that LWM2M server uses:
        // - GET: READ operation
        // - PUT: WRITE operation (already set by init2)
        // - POST: EXECUTE operation, also used for CREATE
        // - DELETE: DELETE operation
        coap_register_handler(unknown_resource, COAP_REQUEST_GET,
                              &CoapClientImpl::incoming_request_handler);
        coap_register_handler(unknown_resource, COAP_REQUEST_POST,
                              &CoapClientImpl::incoming_request_handler);
        coap_register_handler(unknown_resource, COAP_REQUEST_DELETE,
                              &CoapClientImpl::incoming_request_handler);

        coap_add_resource(ctx_, unknown_resource);
    }

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

    if (use_dtls && !psk_identity_.empty()) {
        // Create DTLS session with PSK using libcoap v4.3+ API
        coap_dtls_cpsk_t psk_setup;
        std::memset(&psk_setup, 0, sizeof(psk_setup));
        psk_setup.version = COAP_DTLS_CPSK_SETUP_VERSION;

        // Setup identity
        psk_setup.psk_info.identity.s =
            reinterpret_cast<const uint8_t*>(psk_identity_.c_str());
        psk_setup.psk_info.identity.length = psk_identity_.size();

        // Setup key
        psk_setup.psk_info.key.s = psk_key_.data();
        psk_setup.psk_info.key.length = psk_key_.size();

        session_ = coap_new_client_session_psk2(
            ctx_,
            nullptr,    // local_if - let OS choose
            &dst,       // server address
            proto,      // COAP_PROTO_DTLS
            &psk_setup
        );
    } else {
        // NoSec mode or non-DTLS connection
        session_ = coap_new_client_session(ctx_, nullptr, &dst, proto);
    }

    if (!session_) {
        disconnect();
        return Err<void>(ErrorCode::ConnectionFailed, "Failed to create CoAP session");
    }

    // Store this pointer for callbacks - use context for incoming requests
    // and session for response handlers
    coap_session_set_app_data(session_, this);
    coap_set_app_data(ctx_, this);

    connected_ = true;
    return Ok();
}

void CoapClientImpl::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(io_mtx_);
    if (session_) {
        coap_session_release(session_);
        session_ = nullptr;
    }

    if (ctx_) {
        coap_free_context(ctx_);
        ctx_ = nullptr;
        // Only cleanup libcoap if we had a context (meaning we called init)
        cleanup_libcoap();
    }

    // Clear sensitive credentials
    psk_identity_.clear();
    if (!psk_key_.empty()) {
        std::memset(psk_key_.data(), 0, psk_key_.size());
        psk_key_.clear();
    }

    connected_ = false;
}

bool CoapClientImpl::is_connected() const noexcept {
    return connected_.load();
}

Result<CoapResponse> CoapClientImpl::send(
    const CoapRequest& request,
    std::chrono::milliseconds timeout
) {
    std::lock_guard<std::recursive_mutex> lock(io_mtx_);

    if (!is_connected()) {
        return Err<CoapResponse>(ErrorCode::InvalidState, "Not connected");
    }

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

    // Unique token so a stale/duplicate response isn't taken for this reply.
    uint8_t token[8];
    uint64_t tok_val = ++next_token_;
    std::memcpy(token, &tok_val, sizeof(token));
    coap_add_token(pdu, sizeof(token), token);

    // Add URI path and query options
    coap_uri_t uri;
    std::string full_uri = "coap://localhost" + request.uri_path;
    if (coap_split_uri(
            reinterpret_cast<const uint8_t*>(full_uri.c_str()),
            full_uri.length(),
            &uri
        ) == 0) {
        // Add URI-Path options (one per path segment)
        // and URI-Query options (one per query parameter)
        coap_optlist_t* optlist = nullptr;

        // Add path segments as separate options
        if (uri.path.length > 0 && uri.path.s != nullptr) {
            const uint8_t* p = uri.path.s;
            size_t remaining = uri.path.length;

            // Manual parsing of path segments
            while (remaining > 0) {
                // Skip leading slash
                if (*p == '/') {
                    p++;
                    remaining--;
                    continue;
                }

                // Find end of segment
                const uint8_t* seg_end = p;
                size_t seg_len = 0;
                while (seg_len < remaining && *seg_end != '/') {
                    seg_end++;
                    seg_len++;
                }

                if (seg_len > 0) {
                    coap_insert_optlist(&optlist, coap_new_optlist(
                        COAP_OPTION_URI_PATH, seg_len, p
                    ));
                }

                p = seg_end;
                remaining -= seg_len;
            }
        }

        // Add query parameters as separate options
        if (uri.query.length > 0 && uri.query.s != nullptr) {
            const uint8_t* q = uri.query.s;
            size_t remaining = uri.query.length;

            while (remaining > 0) {
                // Find end of query parameter (at & or end)
                const uint8_t* param_end = q;
                size_t param_len = 0;
                while (param_len < remaining && *param_end != '&') {
                    param_end++;
                    param_len++;
                }

                if (param_len > 0) {
                    coap_insert_optlist(&optlist, coap_new_optlist(
                        COAP_OPTION_URI_QUERY, param_len, q
                    ));
                }

                q = param_end;
                remaining -= param_len;
                // Skip &
                if (remaining > 0 && *q == '&') {
                    q++;
                    remaining--;
                }
            }
        }

        // Add all options to PDU
        if (optlist) {
            coap_add_optlist_pdu(pdu, &optlist);
            coap_delete_optlist(optlist);
        }
    }

    // Add content format
    uint8_t cf_buf[2];
    size_t cf_len = coap_encode_var_safe(cf_buf, sizeof(cf_buf),
                                          static_cast<uint16_t>(request.content_format));
    coap_add_option(pdu, COAP_OPTION_CONTENT_FORMAT, cf_len, cf_buf);

    // Add payload
    if (!request.payload.empty()) {
        // 0 = body doesn't fit the PDU; no block-wise here, so fail vs truncate.
        if (coap_add_data(pdu, request.payload.size(), request.payload.data()) == 0) {
            coap_delete_pdu(pdu);
            return Err<CoapResponse>(ErrorCode::InternalServerError,
                "Payload does not fit in a single PDU (block-wise not supported)");
        }
    }

    // Setup response tracking
    PendingResponse pending;
    pending.token.assign(token, token + sizeof(token));
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
        poll_unlocked(std::min(remaining, std::chrono::milliseconds{100}));
    }

    current_response_ = nullptr;
    return Ok(std::move(pending.response));
}

void CoapClientImpl::set_request_handler(RequestHandler handler) {
    request_handler_ = std::move(handler);
}

void CoapClientImpl::poll(std::chrono::milliseconds timeout) {
    std::lock_guard<std::recursive_mutex> lock(io_mtx_);
    poll_unlocked(timeout);
}

void CoapClientImpl::poll_unlocked(std::chrono::milliseconds timeout) {
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

    coap_bin_const_t tok = coap_pdu_get_token(received);
    const auto& expected = impl->current_response_->token;
    if (tok.length != expected.size() ||
        (tok.length != 0 && std::memcmp(tok.s, expected.data(), tok.length) != 0)) {
        return COAP_RESPONSE_OK;
    }

    // Extract response code
    impl->current_response_->response.code =
        static_cast<CoapCode>(coap_pdu_get_code(received));

    // Extract options (especially Location-Path for registration)
    coap_opt_iterator_t opt_iter;
    coap_opt_t* option;

    coap_option_iterator_init(received, &opt_iter, COAP_OPT_ALL);
    while ((option = coap_option_next(&opt_iter))) {
        // Store Location-Path (8), Location-Query (15), and Content-Format (12)
        if (opt_iter.number == COAP_OPTION_LOCATION_PATH ||
            opt_iter.number == COAP_OPTION_LOCATION_QUERY ||
            opt_iter.number == COAP_OPTION_CONTENT_FORMAT) {
            std::vector<uint8_t> opt_value(
                coap_opt_value(option),
                coap_opt_value(option) + coap_opt_length(option)
            );
            impl->current_response_->response.options.emplace_back(
                opt_iter.number,
                std::move(opt_value)
            );
        }
    }

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

void CoapClientImpl::incoming_request_handler(
    coap_resource_t* resource,
    [[maybe_unused]] coap_session_t* session,
    const coap_pdu_t* request,
    [[maybe_unused]] const coap_string_t* query,
    coap_pdu_t* response
) {
    fprintf(stderr, "DEBUG: incoming_request_handler called, request type=%u\n",
            coap_pdu_get_type(request));

    // Get our implementation from resource's userdata
    auto* impl = static_cast<CoapClientImpl*>(coap_resource_get_userdata(resource));
    if (!impl || !impl->request_handler_) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_NOT_FOUND);
        return;
    }

    // Build CoapRequest from incoming PDU
    CoapRequest coap_request;

    // Extract method
    coap_pdu_code_t pdu_code = coap_pdu_get_code(request);
    switch (pdu_code) {
        case COAP_REQUEST_CODE_GET:
            coap_request.method = CoapMethod::Get;
            break;
        case COAP_REQUEST_CODE_POST:
            coap_request.method = CoapMethod::Post;
            break;
        case COAP_REQUEST_CODE_PUT:
            coap_request.method = CoapMethod::Put;
            break;
        case COAP_REQUEST_CODE_DELETE:
            coap_request.method = CoapMethod::Delete;
            break;
        default:
            coap_pdu_set_code(response, COAP_RESPONSE_CODE_NOT_ALLOWED);
            return;
    }

    // Extract URI path from options
    std::string uri_path;
    coap_opt_iterator_t opt_iter;
    coap_opt_t* option;

    coap_option_iterator_init(request, &opt_iter, COAP_OPT_ALL);
    while ((option = coap_option_next(&opt_iter))) {
        if (opt_iter.number == COAP_OPTION_URI_PATH) {
            if (!uri_path.empty()) {
                uri_path += "/";
            }
            uri_path.append(
                reinterpret_cast<const char*>(coap_opt_value(option)),
                coap_opt_length(option)
            );
        } else if (opt_iter.number == COAP_OPTION_CONTENT_FORMAT) {
            if (coap_opt_length(option) > 0) {
                coap_request.content_format = static_cast<ContentFormat>(
                    coap_decode_var_bytes(coap_opt_value(option), coap_opt_length(option))
                );
            }
        } else if (opt_iter.number == COAP_OPTION_ACCEPT) {
            coap_request.accept = static_cast<ContentFormat>(
                coap_decode_var_bytes(coap_opt_value(option), coap_opt_length(option))
            );
        }
    }

    // Prepend / to path
    if (!uri_path.empty()) {
        uri_path = "/" + uri_path;
    }
    coap_request.uri_path = std::move(uri_path);

    // Extract payload
    const uint8_t* data = nullptr;
    size_t data_len = 0;
    if (coap_get_data(request, &data_len, &data) && data_len > 0) {
        coap_request.payload.assign(data, data + data_len);
    }

    // Call the request handler and get response
    CoapResponse coap_response = impl->request_handler_(coap_request);

    fprintf(stderr, "DEBUG: Handler returned code %u, payload size %zu\n",
            static_cast<unsigned>(coap_response.code), coap_response.payload.size());

    coap_pdu_set_code(response, static_cast<coap_pdu_code_t>(coap_response.code));

    // Add Content-Format option if we have payload
    if (!coap_response.payload.empty()) {
        uint16_t content_format = static_cast<uint16_t>(coap_response.content_format);
        fprintf(stderr, "DEBUG: Adding Content-Format %u and %zu bytes payload\n",
                content_format, coap_response.payload.size());

        // Hex dump payload
        fprintf(stderr, "DEBUG: Payload hex dump: ");
        for (size_t i = 0; i < coap_response.payload.size() && i < 64; ++i) {
            fprintf(stderr, "%02x ", coap_response.payload[i]);
        }
        fprintf(stderr, "\n");

        // Add Content-Format option
        uint8_t cf_buf[2];
        size_t cf_len = coap_encode_var_safe(cf_buf, sizeof(cf_buf), content_format);
        coap_add_option(response, COAP_OPTION_CONTENT_FORMAT, cf_len, cf_buf);

        // Add payload
        int add_result = coap_add_data(response, coap_response.payload.size(), coap_response.payload.data());
        fprintf(stderr, "DEBUG: coap_add_data returned %d\n", add_result);

        // Verify payload was added
        const uint8_t* resp_data = nullptr;
        size_t resp_data_len = 0;
        if (coap_get_data(response, &resp_data_len, &resp_data)) {
            fprintf(stderr, "DEBUG: Response PDU contains %zu bytes of payload\n", resp_data_len);
        } else {
            fprintf(stderr, "DEBUG: WARNING: Response PDU has no payload after coap_add_data!\n");
        }
    } else {
        fprintf(stderr, "DEBUG: No payload to add\n");
    }

    fprintf(stderr, "DEBUG: Response PDU code set to %u, type=%u\n",
            coap_pdu_get_code(response), coap_pdu_get_type(response));
}

Result<void> CoapClientImpl::setup_dtls_psk(const PskCredentials& psk) {
    // Store credentials - they will be used when creating session in connect()
    // Credentials must outlive the session lifetime
    if (psk.identity.empty()) {
        return Err<void>(ErrorCode::BadRequest, "PSK identity cannot be empty");
    }
    if (psk.key.empty()) {
        return Err<void>(ErrorCode::BadRequest, "PSK key cannot be empty");
    }

    psk_identity_ = psk.identity;
    psk_key_ = psk.key;
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
