// Ferriot - Main Client Implementation

#include "lwm2m/client.hpp"
#include "lwm2m/objects/security.hpp"
#include "lwm2m/objects/server.hpp"
#include "lwm2m/objects/device.hpp"
#include "lwm2m/codec/tlv.hpp"

#include <sstream>

namespace lwm2m {

Client::Client(ClientConfig config)
    : config_(std::move(config))
    , security_(std::make_shared<objects::SecurityObject>())
    , server_(std::make_shared<objects::ServerObject>())
    , device_(std::make_shared<objects::DeviceObject>())
{
    // Register mandatory objects
    add_object(security_);
    add_object(server_);
    add_object(device_);

    // Set up registration update callback
    server_->set_registration_update_callback([this](uint16_t ssid) {
        (void)update_registration(ssid);  // Ignore result in callback
    });
}

Client::~Client() {
    stop();
}

void Client::add_object(std::shared_ptr<Object> object) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    objects_[object->id().value] = std::move(object);
}

Object* Client::get_object(ObjectId oid) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = objects_.find(oid.value);
    return it != objects_.end() ? it->second.get() : nullptr;
}

const Object* Client::get_object(ObjectId oid) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = objects_.find(oid.value);
    return it != objects_.end() ? it->second.get() : nullptr;
}

objects::SecurityObject& Client::security() {
    return *security_;
}

objects::ServerObject& Client::server() {
    return *server_;
}

objects::DeviceObject& Client::device() {
    return *device_;
}

Result<void> Client::start() {
    if (running_.load()) {
        return Err<void>(ErrorCode::InvalidState, "Client already running");
    }

    running_.store(true);
    set_state(ClientState::Idle);

    // Start event loop thread
    event_thread_ = std::make_unique<std::thread>(&Client::run_event_loop, this);

    return Ok();
}

void Client::stop() {
    running_.store(false);

    if (event_thread_ && event_thread_->joinable()) {
        event_thread_->join();
    }
    event_thread_.reset();

    // Collect SSIDs first to avoid modifying container while iterating
    std::vector<uint16_t> ssids;
    {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        for (const auto& [ssid, reg] : registrations_) {
            ssids.push_back(ssid);
            (void)reg;  // Silence unused warning
        }
    }

    // Deregister from all servers
    for (auto ssid : ssids) {
        (void)deregister(ssid);
    }

    // Disconnect all connections
    {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        for (auto& [id, conn] : connections_) {
            conn->disconnect();
            (void)id;  // Silence unused warning
        }
        connections_.clear();
        registrations_.clear();
    }

    set_state(ClientState::Idle);
}

bool Client::is_running() const noexcept {
    return running_.load();
}

Result<void> Client::register_with_server(uint16_t short_server_id) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    // Find security instance for this server
    auto sec_inst = security_->find_by_short_server_id(short_server_id);
    if (!sec_inst) {
        return Err<void>(ErrorCode::NotFound, "No security instance for server");
    }

    // Find server instance
    auto srv_inst = server_->find_by_short_server_id(short_server_id);
    if (!srv_inst) {
        return Err<void>(ErrorCode::NotFound, "No server instance for server");
    }

    // Create connection if needed
    if (connections_.find(short_server_id) == connections_.end()) {
        auto conn = transport::CoapClient::create();

        // Get connection config from security object
        // Find instance ID by iterating
        InstanceId sec_iid{0};
        for (auto iid : security_->list_instances()) {
            auto* inst = security_->get_instance(iid);
            if (inst && inst->short_server_id == short_server_id) {
                sec_iid = iid;
                break;
            }
        }

        auto conn_config = security_->to_connection_config(sec_iid);
        if (auto res = conn->connect(conn_config); !res) {
            return Err<void>(res.error().code(), "Connection failed: " +
                            std::string(res.error().message()));
        }

        // Set request handler - forward incoming requests from server to our handler
        conn->set_request_handler([this](const transport::CoapRequest& req) {
            return handle_incoming_request(req);
        });

        connections_[short_server_id] = std::move(conn);
    }

    set_state(ClientState::Registering);

    // Build registration payload
    std::string payload = build_registration_payload();

    // Send registration request
    transport::CoapRequest request;
    request.method = transport::CoapMethod::Post;
    request.uri_path = "/rd?ep=" + config_.endpoint_name +
                       "&lt=" + std::to_string(srv_inst->lifetime) +
                       "&lwm2m=1.1&b=" + srv_inst->binding;
    request.payload = std::vector<uint8_t>(payload.begin(), payload.end());
    request.content_format = transport::ContentFormat::LinkFormat;

    auto& conn = connections_[short_server_id];
    auto response = conn->send(request);

    if (!response) {
        set_state(ClientState::Error);
        return Err<void>(response.error().code(), "Registration failed: " +
                        std::string(response.error().message()));
    }

    if (!response.value().is_success()) {
        set_state(ClientState::Error);
        return Err<void>(ErrorCode::BadRequest, "Registration rejected by server");
    }

    // Parse location from response Location-Path options
    std::string location = response.value().get_location_path();
    if (location.empty()) {
        set_state(ClientState::Error);
        return Err<void>(ErrorCode::BadRequest,
            "Server did not return Location-Path in registration response");
    }

    Registration reg;
    reg.location = std::move(location);
    reg.short_server_id = short_server_id;
    reg.registered_at = std::chrono::system_clock::now();
    reg.expires_at = std::chrono::system_clock::now() +
                     std::chrono::seconds{srv_inst->lifetime};

    registrations_[short_server_id] = reg;
    set_state(ClientState::Registered);

    if (callbacks_.on_registered) {
        callbacks_.on_registered(short_server_id, reg);
    }

    return Ok();
}

Result<void> Client::update_registration(uint16_t short_server_id) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = registrations_.find(short_server_id);
    if (it == registrations_.end()) {
        return Err<void>(ErrorCode::NotFound, "Not registered with this server");
    }

    auto conn_it = connections_.find(short_server_id);
    if (conn_it == connections_.end()) {
        return Err<void>(ErrorCode::InvalidState, "No connection to server");
    }

    set_state(ClientState::Updating);

    transport::CoapRequest request;
    request.method = transport::CoapMethod::Post;
    request.uri_path = it->second.location;
    request.content_format = transport::ContentFormat::LinkFormat;

    auto response = conn_it->second->send(request);

    if (!response || !response.value().is_success()) {
        set_state(ClientState::Registered);
        return Err<void>(ErrorCode::BadRequest, "Update failed");
    }

    // Update expiration
    auto srv_inst = server_->find_by_short_server_id(short_server_id);
    if (srv_inst) {
        it->second.expires_at = std::chrono::system_clock::now() +
                                std::chrono::seconds{srv_inst->lifetime};
    }

    set_state(ClientState::Registered);
    return Ok();
}

Result<void> Client::deregister(uint16_t short_server_id) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = registrations_.find(short_server_id);
    if (it == registrations_.end()) {
        return Ok();  // Already not registered
    }

    auto conn_it = connections_.find(short_server_id);
    if (conn_it != connections_.end()) {
        set_state(ClientState::Deregistering);

        transport::CoapRequest request;
        request.method = transport::CoapMethod::Delete;
        request.uri_path = it->second.location;
        request.content_format = transport::ContentFormat::LinkFormat;

        (void)conn_it->second->send(request);  // Best effort, ignore result
    }

    registrations_.erase(it);

    if (callbacks_.on_deregistered) {
        callbacks_.on_deregistered(short_server_id);
    }

    set_state(registrations_.empty() ? ClientState::Idle : ClientState::Registered);
    return Ok();
}

void Client::poll() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    for (auto& [id, conn] : connections_) {
        conn->poll(config_.poll_interval);
        (void)id;  // Silence unused warning
    }

    check_registration_updates();

    // Check connection health (detect lost connections)
    check_connection_health();

    // Handle reconnection if in progress and backoff time has passed
    if (reconnecting_.load() &&
        std::chrono::steady_clock::now() >= next_reconnect_time_) {
        try_reconnect(reconnect_ssid_);
    }
}

ClientState Client::state() const noexcept {
    return state_.load();
}

std::string_view Client::state_string() const noexcept {
    return client_state_to_string(state_.load());
}

std::optional<Registration> Client::get_registration(uint16_t short_server_id) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = registrations_.find(short_server_id);
    if (it != registrations_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<uint16_t> Client::registered_servers() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    std::vector<uint16_t> result;
    result.reserve(registrations_.size());
    for (const auto& [ssid, reg] : registrations_) {
        result.push_back(ssid);
        (void)reg;  // Silence unused warning
    }
    return result;
}

void Client::set_callbacks(ClientCallbacks callbacks) {
    callbacks_ = std::move(callbacks);
}

void Client::run_event_loop() {
    while (running_.load()) {
        poll();
        std::this_thread::sleep_for(config_.poll_interval);
    }
}

transport::CoapResponse Client::handle_incoming_request(const transport::CoapRequest& request) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    // Parse URI path to ObjectPath (e.g., "/3/0/1" -> Object 3, Instance 0, Resource 1)
    auto path_opt = ObjectPath::parse(request.uri_path);
    if (!path_opt) {
        // Invalid path
        transport::CoapResponse response;
        response.code = transport::CoapCode::BadRequest;
        return response;
    }

    transport::CoapResponse response;

    switch (request.method) {
        case transport::CoapMethod::Get:
            response = process_read(*path_opt);
            break;

        case transport::CoapMethod::Put:
            response = process_write(*path_opt, request.payload);
            break;

        case transport::CoapMethod::Post:
            // POST = Execute (resource) or Create (object/instance)
            if (path_opt->is_resource()) {
                response = process_execute(*path_opt, request.payload);
            } else {
                response = process_write(*path_opt, request.payload);
            }
            break;

        case transport::CoapMethod::Delete:
            response = process_delete(*path_opt);
            break;

        default:
            response.code = transport::CoapCode::MethodNotAllowed;
            break;
    }

    return response;
}

transport::CoapResponse Client::process_read(const ObjectPath& path) {
    transport::CoapResponse response;

    // Debug logging
    fprintf(stderr, "DEBUG: process_read called for path: /%u/%u/%u\n",
            path.object_id.value,
            path.instance_id ? path.instance_id->value : 999,
            path.resource_id ? path.resource_id->value : 999);

    // Find object by ID
    Object* obj = get_object(path.object_id);
    if (!obj) {
        fprintf(stderr, "DEBUG: Object %u not found\n", path.object_id.value);
        response.code = transport::CoapCode::NotFound;
        return response;
    }

    fprintf(stderr, "DEBUG: Object %u found: %s\n", path.object_id.value, obj->name().data());

    codec::TlvEncoder encoder;

    if (path.is_object()) {
        // Read entire object - all instances
        std::vector<uint8_t> payload;
        for (auto iid : obj->list_instances()) {
            auto instance_data = encode_instance_tlv(obj, iid);
            payload.insert(payload.end(), instance_data.begin(), instance_data.end());
        }
        response.payload = std::move(payload);
        response.code = transport::CoapCode::Content;

    } else if (path.is_instance()) {
        // Read entire instance
        if (!obj->has_instance(*path.instance_id)) {
            response.code = transport::CoapCode::NotFound;
            return response;
        }
        response.payload = encode_instance_tlv(obj, *path.instance_id);
        response.code = transport::CoapCode::Content;

    } else if (path.is_resource()) {
        // Read single resource
        fprintf(stderr, "DEBUG: Reading resource %u from instance %u\n",
                path.resource_id->value, path.instance_id->value);

        auto result = obj->read_resource(
            *path.instance_id,
            *path.resource_id,
            path.resource_instance_id
        );

        if (!result) {
            fprintf(stderr, "DEBUG: read_resource failed: %s\n", result.error().message().data());
            response.code = error_to_coap_code(result.error().code());
            return response;
        }

        fprintf(stderr, "DEBUG: read_resource succeeded, encoding TLV\n");

        auto encoded = encoder.encode_resource(*path.resource_id, result.value());
        if (encoded) {
            fprintf(stderr, "DEBUG: TLV encoding succeeded, payload size: %zu\n", encoded.value().size());
            response.payload = std::move(encoded.value());
            response.code = transport::CoapCode::Content;
            // content_format is already set to TlvLwm2m by default
        } else {
            fprintf(stderr, "DEBUG: TLV encoding failed: %s\n", encoded.error().message().data());
            response.code = transport::CoapCode::InternalServerError;
        }
    }

    fprintf(stderr, "DEBUG: Returning response with code %u, payload size %zu\n",
            static_cast<unsigned>(response.code), response.payload.size());

    return response;
}

transport::CoapResponse Client::process_write(
    const ObjectPath& path,
    const std::vector<uint8_t>& payload
) {
    transport::CoapResponse response;

    Object* obj = get_object(path.object_id);
    if (!obj) {
        response.code = transport::CoapCode::NotFound;
        return response;
    }

    codec::TlvDecoder decoder;
    auto records = decoder.decode(payload);
    if (!records) {
        response.code = transport::CoapCode::BadRequest;
        return response;
    }

    if (path.is_resource()) {
        // Write single resource
        if (records.value().empty()) {
            response.code = transport::CoapCode::BadRequest;
            return response;
        }

        const auto& record = records.value()[0];

        // Get expected type from object for proper TLV decoding
        ResourceType expected_type = ResourceType::String;  // Default fallback
        if (auto type = obj->get_resource_type(*path.instance_id, *path.resource_id)) {
            expected_type = *type;
        }

        // Decode TLV with correct type
        auto decoded = decoder.decode_resource(record.value, expected_type);
        if (!decoded) {
            response.code = transport::CoapCode::BadRequest;
            return response;
        }
        ResourceValue value = decoded.value();

        auto result = obj->write_resource(
            *path.instance_id,
            *path.resource_id,
            path.resource_instance_id,
            value
        );

        response.code = result ?
            transport::CoapCode::Changed :
            error_to_coap_code(result.error().code());

    } else if (path.is_instance()) {
        // Write multiple resources in instance
        for (const auto& record : records.value()) {
            if (record.type == codec::TlvType::Resource) {
                ResourceId rid{record.id};

                // Get expected type for this resource
                ResourceType expected_type = ResourceType::String;
                if (auto type = obj->get_resource_type(*path.instance_id, rid)) {
                    expected_type = *type;
                }

                // Decode TLV with correct type
                auto decoded = decoder.decode_resource(record.value, expected_type);
                if (decoded) {
                    (void)obj->write_resource(
                        *path.instance_id,
                        rid,
                        std::nullopt,
                        decoded.value()
                    );
                }
            }
        }
        response.code = transport::CoapCode::Changed;

    } else if (path.is_object()) {
        // Create new instance
        auto result = obj->create_instance(std::nullopt);
        if (result) {
            response.code = transport::CoapCode::Created;
        } else {
            response.code = error_to_coap_code(result.error().code());
        }
    }

    return response;
}

transport::CoapResponse Client::process_execute(
    const ObjectPath& path,
    const std::vector<uint8_t>& payload
) {
    transport::CoapResponse response;

    if (!path.is_resource()) {
        response.code = transport::CoapCode::MethodNotAllowed;
        return response;
    }

    Object* obj = get_object(path.object_id);
    if (!obj) {
        response.code = transport::CoapCode::NotFound;
        return response;
    }

    // Convert payload to arguments string
    std::string_view args(
        reinterpret_cast<const char*>(payload.data()),
        payload.size()
    );

    auto result = obj->execute_resource(
        *path.instance_id,
        *path.resource_id,
        args
    );

    response.code = result ?
        transport::CoapCode::Changed :
        error_to_coap_code(result.error().code());

    return response;
}

transport::CoapResponse Client::process_discover(const ObjectPath& path) {
    transport::CoapResponse response;

    Object* obj = get_object(path.object_id);
    if (!obj) {
        response.code = transport::CoapCode::NotFound;
        return response;
    }

    // Build CoRE Link Format response
    std::ostringstream oss;
    bool first = true;

    if (path.is_object()) {
        // Discover all instances and resources
        for (auto iid : obj->list_instances()) {
            for (auto rid : obj->list_resources(iid)) {
                if (!first) oss << ",";
                first = false;
                oss << "</" << path.object_id.value << "/"
                    << iid.value << "/" << rid.value << ">";
            }
        }
    } else if (path.is_instance()) {
        // Discover resources in instance
        for (auto rid : obj->list_resources(*path.instance_id)) {
            if (!first) oss << ",";
            first = false;
            oss << "</" << path.object_id.value << "/"
                << path.instance_id->value << "/" << rid.value << ">";
        }
    } else if (path.is_resource()) {
        // Single resource discover
        oss << "</" << path.object_id.value << "/"
            << path.instance_id->value << "/" << path.resource_id->value << ">";
    }

    std::string link_format = oss.str();
    response.payload = std::vector<uint8_t>(link_format.begin(), link_format.end());
    response.code = transport::CoapCode::Content;

    return response;
}

transport::CoapResponse Client::process_delete(const ObjectPath& path) {
    transport::CoapResponse response;

    if (!path.is_instance()) {
        response.code = transport::CoapCode::MethodNotAllowed;
        return response;
    }

    Object* obj = get_object(path.object_id);
    if (!obj) {
        response.code = transport::CoapCode::NotFound;
        return response;
    }

    auto result = obj->delete_instance(*path.instance_id);
    response.code = result ?
        transport::CoapCode::Deleted :
        error_to_coap_code(result.error().code());

    return response;
}

transport::CoapCode Client::error_to_coap_code(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::NotFound:
            return transport::CoapCode::NotFound;
        case ErrorCode::MethodNotAllowed:
            return transport::CoapCode::MethodNotAllowed;
        case ErrorCode::BadRequest:
            return transport::CoapCode::BadRequest;
        case ErrorCode::Unauthorized:
            return transport::CoapCode::Unauthorized;
        case ErrorCode::Forbidden:
            return transport::CoapCode::Forbidden;
        default:
            return transport::CoapCode::InternalServerError;
    }
}

std::vector<uint8_t> Client::encode_instance_tlv(Object* obj, InstanceId iid) const {
    codec::TlvEncoder encoder;
    std::vector<std::pair<ResourceId, ResourceValue>> resources;

    for (auto rid : obj->list_resources(iid)) {
        auto result = obj->read_resource(iid, rid, std::nullopt);
        if (result) {
            resources.emplace_back(rid, std::move(result.value()));
        }
    }

    auto encoded = encoder.encode_instance(iid, resources);
    return encoded ? std::move(encoded.value()) : std::vector<uint8_t>{};
}

void Client::set_state(ClientState new_state) {
    auto old_state = state_.exchange(new_state);
    if (old_state != new_state && callbacks_.on_state_change) {
        callbacks_.on_state_change(old_state, new_state);
    }
}

void Client::check_registration_updates() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto now = std::chrono::system_clock::now();

    for (auto& [ssid, reg] : registrations_) {
        auto srv_inst = server_->find_by_short_server_id(ssid);
        if (!srv_inst) continue;

        auto lifetime = std::chrono::seconds{srv_inst->lifetime};
        auto threshold = std::chrono::duration_cast<std::chrono::seconds>(
            lifetime * config_.update_trigger_threshold
        );

        auto time_until_expiry = reg.expires_at - now;
        if (time_until_expiry <= lifetime - threshold) {
            (void)update_registration(ssid);
        }
    }
}

std::string Client::build_registration_payload() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    std::ostringstream oss;
    bool first = true;

    for (const auto& [oid, obj] : objects_) {
        for (auto iid : obj->list_instances()) {
            if (!first) {
                oss << ",";
            }
            first = false;
            oss << "</" << oid << "/" << iid.value << ">";
        }
    }

    return oss.str();
}

// =====================================================================
// Reconnection logic
// =====================================================================

void Client::check_connection_health() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    // Skip if not registered or already reconnecting
    if (state_.load() != ClientState::Registered || reconnecting_.load()) {
        return;
    }

    auto now = std::chrono::system_clock::now();

    // Check each registration for expiration
    for (auto& [ssid, reg] : registrations_) {
        // If registration has expired, trigger reconnection
        if (now > reg.expires_at) {
            handle_connection_lost(ssid);
            return;  // Only handle one at a time
        }
    }

    // Also check connections for disconnect (e.g., DTLS session closed)
    for (auto& [ssid, conn] : connections_) {
        if (!conn->is_connected()) {
            handle_connection_lost(ssid);
            return;  // Only handle one at a time
        }
    }
}

void Client::handle_connection_lost(uint16_t ssid) {
    // Already reconnecting?
    if (reconnecting_.exchange(true)) {
        return;
    }

    set_state(ClientState::Error);
    reconnect_attempts_ = 0;
    reconnect_ssid_ = ssid;
    next_reconnect_time_ = std::chrono::steady_clock::now();

    // Remove stale registration
    registrations_.erase(ssid);

    // Notify callbacks
    if (callbacks_.on_connection_lost) {
        callbacks_.on_connection_lost(ssid);
    }
    if (callbacks_.on_error) {
        callbacks_.on_error(ErrorCode::ConnectionFailed, "Connection to server lost");
    }
}

void Client::try_reconnect(uint16_t ssid) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!reconnect_config_.enabled) {
        reconnecting_ = false;
        return;
    }

    // Check max retries (0 = infinite)
    if (reconnect_config_.max_retries > 0 &&
        reconnect_attempts_ >= reconnect_config_.max_retries) {
        reconnecting_ = false;
        if (callbacks_.on_error) {
            callbacks_.on_error(ErrorCode::ConnectionFailed,
                "Max reconnection attempts reached");
        }
        return;
    }

    ++reconnect_attempts_;

    // Notify reconnecting callback
    if (callbacks_.on_reconnecting) {
        callbacks_.on_reconnecting(ssid, reconnect_attempts_);
    }

    // Close existing connection
    auto it = connections_.find(ssid);
    if (it != connections_.end()) {
        it->second->disconnect();
        connections_.erase(it);
    }

    // Attempt re-registration
    auto result = register_with_server(ssid);

    if (result) {
        // Success - reset reconnection state
        reconnecting_ = false;
        reconnect_attempts_ = 0;

        // Notify reconnected callback
        if (callbacks_.on_reconnected) {
            callbacks_.on_reconnected(ssid);
        }
    } else {
        // Failed - schedule next attempt with backoff
        auto backoff = calculate_backoff();
        next_reconnect_time_ = std::chrono::steady_clock::now() + backoff;
    }
}

std::chrono::seconds Client::calculate_backoff() const {
    auto backoff = reconnect_config_.initial_backoff;

    for (uint16_t i = 1; i < reconnect_attempts_; ++i) {
        auto next_backoff = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::duration<double>(static_cast<double>(backoff.count()) * reconnect_config_.backoff_multiplier)
        );
        if (next_backoff > reconnect_config_.max_backoff) {
            backoff = reconnect_config_.max_backoff;
            break;
        }
        backoff = next_backoff;
    }

    return backoff;
}

std::string_view client_state_to_string(ClientState state) noexcept {
    switch (state) {
        case ClientState::Idle: return "Idle";
        case ClientState::Registering: return "Registering";
        case ClientState::Registered: return "Registered";
        case ClientState::Updating: return "Updating";
        case ClientState::Deregistering: return "Deregistering";
        case ClientState::BootstrapPending: return "BootstrapPending";
        case ClientState::Bootstrapping: return "Bootstrapping";
        case ClientState::Error: return "Error";
        default: return "Unknown";
    }
}

} // namespace lwm2m
