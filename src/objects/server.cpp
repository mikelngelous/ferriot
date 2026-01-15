// Ferriot - Server Object Implementation

#include "lwm2m/objects/server.hpp"

namespace lwm2m::objects {

ServerObject::ServerObject() = default;

std::vector<InstanceId> ServerObject::list_instances() const {
    std::vector<InstanceId> result;
    result.reserve(instances_.size());
    for (const auto& [id, _] : instances_) {
        result.push_back(InstanceId{id});
    }
    return result;
}

bool ServerObject::has_instance(InstanceId iid) const {
    return instances_.find(iid.value) != instances_.end();
}

Result<InstanceId> ServerObject::create_instance(std::optional<InstanceId> suggested) {
    uint16_t new_id = suggested.value_or(InstanceId{next_instance_id_}).value;

    if (instances_.find(new_id) != instances_.end()) {
        return Err<InstanceId>(ErrorCode::BadRequest, "Instance already exists");
    }

    instances_[new_id] = ServerInstance{};
    if (new_id >= next_instance_id_) {
        next_instance_id_ = static_cast<uint16_t>(new_id + 1);
    }

    return Ok(InstanceId{new_id});
}

Result<void> ServerObject::delete_instance(InstanceId iid) {
    auto it = instances_.find(iid.value);
    if (it == instances_.end()) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    instances_.erase(it);
    return Ok();
}

std::vector<ResourceId> ServerObject::list_resources(InstanceId /* iid */) const {
    return {
        server_resource::ShortServerId,
        server_resource::Lifetime,
        server_resource::DefaultMinPeriod,
        server_resource::DefaultMaxPeriod,
        server_resource::Disable,
        server_resource::DisableTimeout,
        server_resource::NotificationStoring,
        server_resource::Binding,
        server_resource::RegistrationUpdateTrigger,
    };
}

Result<ResourceValue> ServerObject::read_resource(
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
        case 0:  // Short Server ID
            return Ok<ResourceValue>(static_cast<int64_t>(inst.short_server_id));
        case 1:  // Lifetime
            return Ok<ResourceValue>(static_cast<int64_t>(inst.lifetime));
        case 2:  // Default Minimum Period
            return Ok<ResourceValue>(static_cast<int64_t>(inst.default_min_period));
        case 3:  // Default Maximum Period
            return Ok<ResourceValue>(static_cast<int64_t>(inst.default_max_period));
        case 5:  // Disable Timeout
            return Ok<ResourceValue>(static_cast<int64_t>(inst.disable_timeout));
        case 6:  // Notification Storing When Disabled or Offline
            return Ok<ResourceValue>(inst.notification_storing);
        case 7:  // Binding
            return Ok<ResourceValue>(inst.binding);
        default:
            return Err<ResourceValue>(ErrorCode::NotFound, "Resource not found");
    }
}

Result<void> ServerObject::write_resource(
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
        case 0:  // Short Server ID
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.short_server_id = static_cast<uint16_t>(*i);
                return Ok();
            }
            break;
        case 1:  // Lifetime
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.lifetime = static_cast<uint32_t>(*i);
                return Ok();
            }
            break;
        case 2:  // Default Minimum Period
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.default_min_period = static_cast<uint32_t>(*i);
                return Ok();
            }
            break;
        case 3:  // Default Maximum Period
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.default_max_period = static_cast<uint32_t>(*i);
                return Ok();
            }
            break;
        case 5:  // Disable Timeout
            if (auto* i = std::get_if<int64_t>(&value)) {
                inst.disable_timeout = static_cast<uint32_t>(*i);
                return Ok();
            }
            break;
        case 6:  // Notification Storing
            if (auto* b = std::get_if<bool>(&value)) {
                inst.notification_storing = *b;
                return Ok();
            }
            break;
        case 7:  // Binding
            if (auto* str = std::get_if<std::string>(&value)) {
                inst.binding = *str;
                return Ok();
            }
            break;
        default:
            return Err<void>(ErrorCode::NotFound, "Resource not found");
    }

    return Err<void>(ErrorCode::BadRequest, "Invalid value type");
}

Result<void> ServerObject::execute_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::string_view arguments
) {
    auto it = instances_.find(iid.value);
    if (it == instances_.end()) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 4:  // Disable
            // TODO: Implement disable functionality
            return Ok();

        case 8:  // Registration Update Trigger
            if (registration_update_callback_) {
                registration_update_callback_(it->second.short_server_id);
            }
            return Ok();

        case 9:  // Bootstrap Request Trigger (LWM2M 1.1)
            // TODO: Implement bootstrap trigger
            return Ok();

        default:
            return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not executable");
    }
}

Result<InstanceId> ServerObject::add_server(const ServerInstance& config) {
    auto result = create_instance(std::nullopt);
    if (!result) {
        return result;
    }

    instances_[result.value().value] = config;
    return result;
}

const ServerInstance* ServerObject::get_instance(InstanceId iid) const {
    auto it = instances_.find(iid.value);
    if (it != instances_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::optional<ServerInstance> ServerObject::find_by_short_server_id(uint16_t ssid) const {
    for (const auto& [_, inst] : instances_) {
        if (inst.short_server_id == ssid) {
            return inst;
        }
    }
    return std::nullopt;
}

void ServerObject::set_registration_update_callback(RegistrationUpdateCallback callback) {
    registration_update_callback_ = std::move(callback);
}

} // namespace lwm2m::objects
