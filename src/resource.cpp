// Ferriot - Resource Implementation

#include "lwm2m/resource.hpp"

namespace lwm2m {

ResourceType get_resource_type(const ResourceValue& value) {
    return std::visit([](const auto& v) -> ResourceType {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return ResourceType::None;
        } else if constexpr (std::is_same_v<T, bool>) {
            return ResourceType::Boolean;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return ResourceType::Integer;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return ResourceType::UnsignedInteger;
        } else if constexpr (std::is_same_v<T, double>) {
            return ResourceType::Float;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return ResourceType::String;
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return ResourceType::Opaque;
        } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
            return ResourceType::Time;
        } else if constexpr (std::is_same_v<T, ObjectPath>) {
            return ResourceType::ObjectLink;
        } else {
            return ResourceType::None;
        }
    }, value);
}

Resource::Resource(ResourceDefinition def)
    : def_(std::move(def)) {}

Resource& Resource::on_read(ReadHandler handler) {
    read_handler_ = std::move(handler);
    return *this;
}

Resource& Resource::on_write(WriteHandler handler) {
    write_handler_ = std::move(handler);
    return *this;
}

Resource& Resource::on_execute(ExecuteHandler handler) {
    execute_handler_ = std::move(handler);
    return *this;
}

Result<ResourceValue> Resource::read(
    InstanceId instance_id,
    std::optional<ResourceInstanceId> resource_instance_id
) const {
    if (!has_access(def_.access, ResourceAccess::Read)) {
        return Err<ResourceValue>(ErrorCode::MethodNotAllowed, "Resource is not readable");
    }

    if (!read_handler_) {
        return Err<ResourceValue>(ErrorCode::NotImplemented, "No read handler registered");
    }

    return read_handler_(instance_id, resource_instance_id);
}

Result<void> Resource::write(
    InstanceId instance_id,
    std::optional<ResourceInstanceId> resource_instance_id,
    const ResourceValue& value
) {
    if (!has_access(def_.access, ResourceAccess::Write)) {
        return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not writable");
    }

    if (!write_handler_) {
        return Err<void>(ErrorCode::NotImplemented, "No write handler registered");
    }

    return write_handler_(instance_id, resource_instance_id, value);
}

Result<void> Resource::execute(
    InstanceId instance_id,
    std::string_view arguments
) {
    if (!has_access(def_.access, ResourceAccess::Execute)) {
        return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not executable");
    }

    if (!execute_handler_) {
        return Err<void>(ErrorCode::NotImplemented, "No execute handler registered");
    }

    return execute_handler_(instance_id, arguments);
}

} // namespace lwm2m
