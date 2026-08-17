// Ferriot - Security Object Implementation

#include "lwm2m/objects/security.hpp"

namespace lwm2m::objects {

SecurityObject::SecurityObject() = default;

std::vector<InstanceId> SecurityObject::list_instances() const {
    std::vector<InstanceId> result;
    result.reserve(instances_.size());
    for (const auto& [id, _] : instances_) {
        result.push_back(InstanceId{id});
    }
    return result;
}

bool SecurityObject::has_instance(InstanceId iid) const {
    return instances_.find(iid.value) != instances_.end();
}

Result<InstanceId> SecurityObject::create_instance(std::optional<InstanceId> suggested) {
    uint16_t new_id = suggested.value_or(InstanceId{next_instance_id_}).value;

    if (instances_.find(new_id) != instances_.end()) {
        return Err<InstanceId>(ErrorCode::BadRequest, "Instance already exists");
    }

    instances_[new_id] = SecurityInstance{};
    if (new_id >= next_instance_id_) {
        next_instance_id_ = static_cast<uint16_t>(new_id + 1);
    }

    return Ok(InstanceId{new_id});
}

Result<void> SecurityObject::delete_instance(InstanceId iid) {
    auto it = instances_.find(iid.value);
    if (it == instances_.end()) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    instances_.erase(it);
    return Ok();
}

std::vector<ResourceId> SecurityObject::list_resources(InstanceId /* iid */) const {
    return {
        security_resource::ServerUri,
        security_resource::BootstrapServer,
        security_resource::SecurityMode,
        security_resource::PublicKeyIdentity,
        security_resource::ServerPublicKey,
        security_resource::SecretKey,
        security_resource::ShortServerId,
        security_resource::ClientHoldOffTime,
        security_resource::BootstrapTimeout,
    };
}

std::optional<ResourceType> SecurityObject::get_resource_type(
    [[maybe_unused]] InstanceId iid, ResourceId rid) const {
    switch (rid.value) {
        case 0:  // Server URI
        case 9:  // SMS Server Number
            return ResourceType::String;
        case 1:  // Bootstrap Server
            return ResourceType::Boolean;
        case 2:  // Security Mode
        case 6:  // SMS Security Mode
        case 10: // Short Server ID
        case 11: // Client Hold Off Time
        case 12: // Bootstrap Server Account Timeout
            return ResourceType::Integer;
        case 3:  // Public Key or Identity
        case 4:  // Server Public Key
        case 5:  // Secret Key
        case 7:  // SMS Binding Key Parameters
        case 8:  // SMS Binding Secret Key(s)
            return ResourceType::Opaque;
        default:
            return std::nullopt;
    }
}

Result<ResourceValue> SecurityObject::read_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::optional<ResourceInstanceId> riid
) const {
    auto it = instances_.find(iid.value);
    if (it == instances_.end()) {
        return Err<ResourceValue>(ErrorCode::NotFound, "Instance not found");
    }

    const auto& inst = it->second;

    switch (rid.value) {
        case 0:  // Server URI
            return Ok<ResourceValue>(inst.server_uri);
        case 1:  // Bootstrap Server
            return Ok<ResourceValue>(inst.bootstrap_server);
        case 2:  // Security Mode
            return Ok<ResourceValue>(static_cast<int64_t>(inst.security_mode));
        case 3:  // Public Key or Identity
            return Ok<ResourceValue>(inst.public_key_identity);
        case 4:  // Server Public Key
            return Ok<ResourceValue>(inst.server_public_key);
        case 5:  // Secret Key
            return Ok<ResourceValue>(inst.secret_key);
        case 10: // Short Server ID
            return Ok<ResourceValue>(static_cast<int64_t>(inst.short_server_id));
        case 11: // Client Hold Off Time
            return Ok<ResourceValue>(static_cast<int64_t>(inst.client_hold_off_time));
        case 12: // Bootstrap Server Account Timeout
            return Ok<ResourceValue>(static_cast<int64_t>(inst.bootstrap_timeout));
        default:
            return Err<ResourceValue>(ErrorCode::NotFound, "Resource not found");
    }
}

Result<void> SecurityObject::write_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::optional<ResourceInstanceId> riid,
    const ResourceValue& value
) {
    auto it = instances_.find(iid.value);
    if (it == instances_.end()) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    auto& inst = it->second;

    switch (rid.value) {
        case 0:  // Server URI
            if (auto* str = std::get_if<std::string>(&value)) {
                inst.server_uri = *str;
                return Ok();
            }
            break;
        case 1:  // Bootstrap Server
            if (auto* b = std::get_if<bool>(&value)) {
                inst.bootstrap_server = *b;
                return Ok();
            }
            break;
        case 2:  // Security Mode
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.security_mode = static_cast<SecurityModeValue>(*i);
                return Ok();
            }
            break;
        case 3:  // Public Key or Identity
            if (auto* v = std::get_if<std::vector<uint8_t>>(&value)) {
                inst.public_key_identity = *v;
                return Ok();
            }
            break;
        case 4:  // Server Public Key
            if (auto* v = std::get_if<std::vector<uint8_t>>(&value)) {
                inst.server_public_key = *v;
                return Ok();
            }
            break;
        case 5:  // Secret Key
            if (auto* v = std::get_if<std::vector<uint8_t>>(&value)) {
                inst.secret_key = *v;
                return Ok();
            }
            break;
        case 10: // Short Server ID
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.short_server_id = static_cast<uint16_t>(*i);
                return Ok();
            }
            break;
        case 11: // Client Hold Off Time
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.client_hold_off_time = static_cast<uint32_t>(*i);
                return Ok();
            }
            break;
        case 12: // Bootstrap Server Account Timeout
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.bootstrap_timeout = static_cast<uint32_t>(*i);
                return Ok();
            }
            break;
        default:
            return Err<void>(ErrorCode::NotFound, "Resource not found");
    }

    return Err<void>(ErrorCode::BadRequest, "Invalid value type");
}

Result<InstanceId> SecurityObject::add_server(const SecurityInstance& config) {
    auto result = create_instance(std::nullopt);
    if (!result) {
        return result;
    }

    instances_[result.value().value] = config;
    return result;
}

const SecurityInstance* SecurityObject::get_instance(InstanceId iid) const {
    auto it = instances_.find(iid.value);
    if (it != instances_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::optional<SecurityInstance> SecurityObject::find_by_short_server_id(uint16_t ssid) const {
    for (const auto& [_, inst] : instances_) {
        if (inst.short_server_id == ssid) {
            return inst;
        }
    }
    return std::nullopt;
}

transport::ConnectionConfig SecurityObject::to_connection_config(InstanceId iid) const {
    transport::ConnectionConfig config;

    auto it = instances_.find(iid.value);
    if (it == instances_.end()) {
        return config;
    }

    const auto& inst = it->second;
    config.server_uri = inst.server_uri;

    switch (inst.security_mode) {
        case SecurityModeValue::NoSec:
            config.security_mode = transport::SecurityMode::NoSec;
            break;

        case SecurityModeValue::PreSharedKey:
            config.security_mode = transport::SecurityMode::Psk;
            {
                transport::PskCredentials psk_creds;
                psk_creds.identity = std::string(inst.public_key_identity.begin(),
                                                 inst.public_key_identity.end());
                psk_creds.key = inst.secret_key;
                config.psk = psk_creds;
            }
            break;

        case SecurityModeValue::RawPublicKey:
            config.security_mode = transport::SecurityMode::Rpk;
            {
                transport::RpkCredentials rpk_creds;
                rpk_creds.public_key = inst.public_key_identity;
                rpk_creds.private_key = inst.secret_key;
                rpk_creds.server_public_key = inst.server_public_key;
                config.rpk = rpk_creds;
            }
            break;

        case SecurityModeValue::Certificate:
        case SecurityModeValue::CertificateWithEst:
            config.security_mode = transport::SecurityMode::Certificate;
            break;
    }

    return config;
}

} // namespace lwm2m::objects
