// Leshan REST API Client Implementation

#include "leshan_api.hpp"

#include <curl/curl.h>
#include <sstream>
#include <thread>

namespace lwm2m::test {

namespace {

// CURL write callback
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

// RAII wrapper for CURL
class CurlHandle {
public:
    CurlHandle() : handle_(curl_easy_init()) {}
    ~CurlHandle() {
        if (handle_) {
            curl_easy_cleanup(handle_);
        }
    }

    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;

    CURL* get() { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    CURL* handle_;
};

} // anonymous namespace

LeshanApi::LeshanApi(std::string base_url)
    : base_url_(std::move(base_url)) {
    // Initialize CURL globally (safe to call multiple times)
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LeshanApi::~LeshanApi() {
    // Note: curl_global_cleanup() should be called once at program exit
    // We don't call it here as other instances might still be using CURL
}

bool LeshanApi::is_server_available() const {
    auto result = http_get("/api/clients");
    return result.has_value();
}

std::vector<LeshanClient> LeshanApi::get_clients() const {
    std::vector<LeshanClient> clients;

    auto response = http_get("/api/clients");
    if (!response) {
        return clients;
    }

    auto json = parse_json(*response);
    if (!json || !json->isArray()) {
        last_error_ = "Invalid JSON response: expected array";
        return clients;
    }

    for (const auto& item : *json) {
        clients.push_back(parse_client(item));
    }

    return clients;
}

std::optional<LeshanClient> LeshanApi::get_client(const std::string& endpoint) const {
    auto response = http_get("/api/clients/" + endpoint);
    if (!response) {
        return std::nullopt;
    }

    auto json = parse_json(*response);
    if (!json) {
        return std::nullopt;
    }

    return parse_client(*json);
}

bool LeshanApi::client_exists(const std::string& endpoint) const {
    return get_client(endpoint).has_value();
}

bool LeshanApi::wait_for_client(const std::string& endpoint,
                                 std::chrono::seconds timeout) const {
    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < timeout) {
        if (client_exists(endpoint)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }

    last_error_ = "Timeout waiting for client: " + endpoint;
    return false;
}

bool LeshanApi::wait_for_client_gone(const std::string& endpoint,
                                      std::chrono::seconds timeout) const {
    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < timeout) {
        if (!client_exists(endpoint)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }

    last_error_ = "Timeout waiting for client to deregister: " + endpoint;
    return false;
}

std::optional<LeshanResourceValue> LeshanApi::read_resource(
    const std::string& endpoint,
    uint16_t object_id,
    uint16_t instance_id,
    uint16_t resource_id
) const {
    std::ostringstream path;
    path << "/api/clients/" << endpoint << "/"
         << object_id << "/" << instance_id << "/" << resource_id;

    auto response = http_get(path.str());
    if (!response) {
        return std::nullopt;
    }

    auto json = parse_json(*response);
    if (!json) {
        return std::nullopt;
    }

    // Check for error in response
    if (json->isMember("failure") && (*json)["failure"].asBool()) {
        last_error_ = "Read failed: " + (*json)["errorMessage"].asString();
        return std::nullopt;
    }

    // Parse the content
    if (json->isMember("content")) {
        return parse_resource_value((*json)["content"]);
    }

    return std::nullopt;
}

bool LeshanApi::write_resource(
    const std::string& endpoint,
    uint16_t object_id,
    uint16_t instance_id,
    uint16_t resource_id,
    const LeshanResourceValue& value
) const {
    std::ostringstream path;
    path << "/api/clients/" << endpoint << "/"
         << object_id << "/" << instance_id << "/" << resource_id;

    // Build JSON payload for Leshan REST API
    // Required fields: id, kind, value, type
    Json::Value payload;
    payload["id"] = resource_id;
    payload["kind"] = "singleResource";

    switch (value.type) {
        case LeshanResourceValue::Type::String:
            payload["value"] = value.string_value;
            payload["type"] = "STRING";
            break;
        case LeshanResourceValue::Type::Integer:
            payload["value"] = static_cast<Json::Int64>(value.int_value);
            payload["type"] = "INTEGER";
            break;
        case LeshanResourceValue::Type::Float:
            payload["value"] = value.float_value;
            payload["type"] = "FLOAT";
            break;
        case LeshanResourceValue::Type::Boolean:
            payload["value"] = value.bool_value;
            payload["type"] = "BOOLEAN";
            break;
        default:
            last_error_ = "Unsupported value type for write";
            return false;
    }

    Json::StreamWriterBuilder writer;
    std::string body = Json::writeString(writer, payload);

    auto response = http_put(path.str(), body);
    if (!response) {
        return false;
    }

    auto json = parse_json(*response);
    if (json && json->isMember("failure") && (*json)["failure"].asBool()) {
        last_error_ = "Write failed: " + (*json)["errorMessage"].asString();
        return false;
    }

    return true;
}

bool LeshanApi::execute_resource(
    const std::string& endpoint,
    uint16_t object_id,
    uint16_t instance_id,
    uint16_t resource_id,
    const std::string& arguments
) const {
    std::ostringstream path;
    path << "/api/clients/" << endpoint << "/"
         << object_id << "/" << instance_id << "/" << resource_id;

    auto response = http_post(path.str(), arguments);
    if (!response) {
        return false;
    }

    auto json = parse_json(*response);
    if (json && json->isMember("failure") && (*json)["failure"].asBool()) {
        last_error_ = "Execute failed: " + (*json)["errorMessage"].asString();
        return false;
    }

    return true;
}

bool LeshanApi::observe(
    const std::string& endpoint,
    uint16_t object_id,
    uint16_t instance_id,
    uint16_t resource_id
) const {
    std::ostringstream path;
    path << "/api/clients/" << endpoint << "/"
         << object_id << "/" << instance_id << "/" << resource_id << "/observe";

    auto response = http_post(path.str());
    return response.has_value();
}

bool LeshanApi::cancel_observe(
    const std::string& endpoint,
    uint16_t object_id,
    uint16_t instance_id,
    uint16_t resource_id
) const {
    std::ostringstream path;
    path << "/api/clients/" << endpoint << "/"
         << object_id << "/" << instance_id << "/" << resource_id << "/observe";

    return http_delete(path.str());
}

bool LeshanApi::delete_client(const std::string& endpoint) const {
    return http_delete("/api/clients/" + endpoint);
}

std::optional<std::string> LeshanApi::http_get(const std::string& path) const {
    CurlHandle curl;
    if (!curl) {
        last_error_ = "Failed to initialize CURL";
        return std::nullopt;
    }

    std::string response;
    std::string url = base_url_ + path;

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) {
        last_error_ = "HTTP GET failed: " + std::string(curl_easy_strerror(res));
        return std::nullopt;
    }

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        last_error_ = "HTTP GET returned " + std::to_string(http_code);
        return std::nullopt;
    }

    return response;
}

std::optional<std::string> LeshanApi::http_post(const std::string& path,
                                                 const std::string& body) const {
    CurlHandle curl;
    if (!curl) {
        last_error_ = "Failed to initialize CURL";
        return std::nullopt;
    }

    std::string response;
    std::string url = base_url_ + path;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl.get());
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        last_error_ = "HTTP POST failed: " + std::string(curl_easy_strerror(res));
        return std::nullopt;
    }

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        last_error_ = "HTTP POST returned " + std::to_string(http_code);
        return std::nullopt;
    }

    return response;
}

std::optional<std::string> LeshanApi::http_put(const std::string& path,
                                                const std::string& body) const {
    CurlHandle curl;
    if (!curl) {
        last_error_ = "Failed to initialize CURL";
        return std::nullopt;
    }

    std::string response;
    std::string url = base_url_ + path;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl.get());
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        last_error_ = "HTTP PUT failed: " + std::string(curl_easy_strerror(res));
        return std::nullopt;
    }

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        last_error_ = "HTTP PUT returned " + std::to_string(http_code);
        return std::nullopt;
    }

    return response;
}

bool LeshanApi::http_delete(const std::string& path) const {
    CurlHandle curl;
    if (!curl) {
        last_error_ = "Failed to initialize CURL";
        return false;
    }

    std::string response;
    std::string url = base_url_ + path;

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) {
        last_error_ = "HTTP DELETE failed: " + std::string(curl_easy_strerror(res));
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        last_error_ = "HTTP DELETE returned " + std::to_string(http_code);
        return false;
    }

    return true;
}

std::optional<Json::Value> LeshanApi::parse_json(const std::string& json_str) const {
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(json_str);

    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        last_error_ = "JSON parse error: " + errors;
        return std::nullopt;
    }

    return root;
}

LeshanClient LeshanApi::parse_client(const Json::Value& json) const {
    LeshanClient client;

    client.endpoint = json.get("endpoint", "").asString();
    client.registration_id = json.get("registrationId", "").asString();
    client.address = json.get("address", "").asString();
    client.lwm2m_version = json.get("lwM2mVersion", "1.0").asString();
    client.lifetime = json.get("lifetime", 86400).asUInt();
    client.binding_mode = json.get("bindingMode", "U").asString();

    // Parse timestamps (milliseconds since epoch)
    auto reg_date = json.get("registrationDate", 0).asInt64();
    auto last_update = json.get("lastUpdate", 0).asInt64();
    client.registration_date = std::chrono::system_clock::from_time_t(reg_date / 1000);
    client.last_update = std::chrono::system_clock::from_time_t(last_update / 1000);

    // Parse object links
    if (json.isMember("objectLinks") && json["objectLinks"].isArray()) {
        for (const auto& link : json["objectLinks"]) {
            client.object_links.push_back(link.get("url", "").asString());
        }
    }

    return client;
}

LeshanResourceValue LeshanApi::parse_resource_value(const Json::Value& json) const {
    LeshanResourceValue value;

    // Leshan includes a "type" field that indicates the LWM2M resource type
    // The "value" field may be serialized as string even for integers
    std::string lwm2m_type;
    if (json.isMember("type")) {
        lwm2m_type = json["type"].asString();
    }

    if (json.isMember("value")) {
        const auto& val = json["value"];

        // Use LWM2M type hint if available, otherwise fall back to JSON type
        if (lwm2m_type == "INTEGER" || lwm2m_type == "UNSIGNED_INTEGER") {
            value.type = LeshanResourceValue::Type::Integer;
            if (val.isString()) {
                value.int_value = std::stoll(val.asString());
            } else {
                value.int_value = val.asInt64();
            }
        } else if (lwm2m_type == "FLOAT") {
            value.type = LeshanResourceValue::Type::Float;
            if (val.isString()) {
                value.float_value = std::stod(val.asString());
            } else {
                value.float_value = val.asDouble();
            }
        } else if (lwm2m_type == "BOOLEAN") {
            value.type = LeshanResourceValue::Type::Boolean;
            if (val.isString()) {
                value.bool_value = (val.asString() == "true");
            } else {
                value.bool_value = val.asBool();
            }
        } else if (lwm2m_type == "STRING" || lwm2m_type == "OPAQUE" || lwm2m_type == "TIME" || lwm2m_type == "OBJLNK") {
            value.type = LeshanResourceValue::Type::String;
            value.string_value = val.asString();
        } else {
            // Fallback: infer type from JSON value type
            if (val.isString()) {
                value.type = LeshanResourceValue::Type::String;
                value.string_value = val.asString();
            } else if (val.isInt64()) {
                value.type = LeshanResourceValue::Type::Integer;
                value.int_value = val.asInt64();
            } else if (val.isDouble()) {
                value.type = LeshanResourceValue::Type::Float;
                value.float_value = val.asDouble();
            } else if (val.isBool()) {
                value.type = LeshanResourceValue::Type::Boolean;
                value.bool_value = val.asBool();
            }
        }
    }

    return value;
}

} // namespace lwm2m::test
