// Ferriot - Firmware Update Object Implementation

#include "lwm2m/objects/firmware_update.hpp"

namespace lwm2m::objects {

FirmwareUpdateObject::FirmwareUpdateObject() = default;

std::vector<ResourceId> FirmwareUpdateObject::list_resources(InstanceId /* iid */) const {
    return {
        firmware_resource::Package,
        firmware_resource::PackageUri,
        firmware_resource::Update,
        firmware_resource::State,
        firmware_resource::UpdateResult,
        firmware_resource::PkgName,
        firmware_resource::PkgVersion,
        firmware_resource::ProtocolSupport,
        firmware_resource::DeliveryMethod,
    };
}

Result<ResourceValue> FirmwareUpdateObject::read_resource(
    InstanceId iid,
    ResourceId rid,
    std::optional<ResourceInstanceId> riid
) const {
    if (iid.value != 0) {
        return Err<ResourceValue>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 0:  // Package (write-only)
            return Err<ResourceValue>(ErrorCode::MethodNotAllowed, "Package is write-only");
        case 1:  // Package URI (write-only for security)
            return Err<ResourceValue>(ErrorCode::MethodNotAllowed, "Package URI is write-only");
        case 3:  // State
            return Ok<ResourceValue>(static_cast<int64_t>(state_));
        case 5:  // Update Result
            return Ok<ResourceValue>(static_cast<int64_t>(update_result_));
        case 6:  // Package Name
            return Ok<ResourceValue>(package_name_);
        case 7:  // Package Version
            return Ok<ResourceValue>(package_version_);
        case 8:  // Protocol Support (multiple)
            if (riid.has_value()) {
                if (riid->value < supported_protocols_.size()) {
                    return Ok<ResourceValue>(
                        static_cast<int64_t>(supported_protocols_[riid->value])
                    );
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!supported_protocols_.empty()) {
                return Ok<ResourceValue>(
                    static_cast<int64_t>(supported_protocols_[0])
                );
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No protocols defined");
        case 9:  // Delivery Method
            return Ok<ResourceValue>(static_cast<int64_t>(delivery_method_));
        default:
            return Err<ResourceValue>(ErrorCode::NotFound, "Resource not found");
    }
}

Result<void> FirmwareUpdateObject::write_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::optional<ResourceInstanceId> riid,
    const ResourceValue& value
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 0: {  // Package
            // Can only write package in Idle or Downloaded state
            if (state_ != FirmwareState::Idle && state_ != FirmwareState::Downloaded) {
                return Err<void>(ErrorCode::MethodNotAllowed, "Cannot write package in current state");
            }

            if (auto* data = std::get_if<std::vector<uint8_t>>(&value)) {
                return push_package(*data);
            }
            return Err<void>(ErrorCode::BadRequest, "Invalid value type");
        }
        case 1: {  // Package URI
            // Can only write URI in Idle state
            if (state_ != FirmwareState::Idle) {
                return Err<void>(ErrorCode::MethodNotAllowed, "Cannot write URI in current state");
            }

            if (auto* uri = std::get_if<std::string>(&value)) {
                package_uri_ = *uri;
                update_result_ = FirmwareUpdateResult::Initial;

                // Start download if callback is set
                if (callbacks_.start_download) {
                    auto result = callbacks_.start_download(*uri);
                    if (result) {
                        state_ = FirmwareState::Downloading;
                    } else {
                        update_result_ = FirmwareUpdateResult::InvalidUri;
                        return result;
                    }
                }
                return Ok();
            }
            return Err<void>(ErrorCode::BadRequest, "Invalid value type");
        }
        default:
            return Err<void>(ErrorCode::MethodNotAllowed, "Resource is read-only");
    }
}

Result<void> FirmwareUpdateObject::execute_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::string_view arguments
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 2: {  // Update (Execute)
            // Can only execute update in Downloaded state
            if (state_ != FirmwareState::Downloaded) {
                return Err<void>(ErrorCode::MethodNotAllowed, "Cannot update: not in Downloaded state");
            }

            if (callbacks_.apply_update) {
                state_ = FirmwareState::Updating;
                auto result = callbacks_.apply_update();
                if (!result) {
                    state_ = FirmwareState::Idle;
                    update_result_ = FirmwareUpdateResult::UpdateFailed;
                    return result;
                }
                // Note: Device will typically reboot, so we may not reach here
            }
            return Ok();
        }
        default:
            return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not executable");
    }
}

void FirmwareUpdateObject::set_callbacks(FirmwareUpdateCallbacks callbacks) {
    callbacks_ = std::move(callbacks);
}

void FirmwareUpdateObject::set_state(FirmwareState state) {
    state_ = state;
}

void FirmwareUpdateObject::set_update_result(FirmwareUpdateResult result) {
    update_result_ = result;
}

void FirmwareUpdateObject::set_package_info(const std::string& name, const std::string& version) {
    package_name_ = name;
    package_version_ = version;
}

void FirmwareUpdateObject::set_supported_protocols(std::vector<FirmwareProtocol> protocols) {
    supported_protocols_ = std::move(protocols);
}

void FirmwareUpdateObject::set_delivery_method(FirmwareDeliveryMethod method) {
    delivery_method_ = method;
}

Result<void> FirmwareUpdateObject::push_package(const std::vector<uint8_t>& data) {
    if (state_ != FirmwareState::Idle && state_ != FirmwareState::Downloaded) {
        return Err<void>(ErrorCode::MethodNotAllowed, "Cannot push package in current state");
    }

    package_data_ = data;
    update_result_ = FirmwareUpdateResult::Initial;

    // Verify package if callback is set
    if (callbacks_.verify_package) {
        auto result = callbacks_.verify_package(data);
        if (!result) {
            package_data_.clear();
            update_result_ = FirmwareUpdateResult::IntegrityCheckFailure;
            return result;
        }
    }

    state_ = FirmwareState::Downloaded;
    return Ok();
}

void FirmwareUpdateObject::download_complete(bool success) {
    if (state_ != FirmwareState::Downloading) {
        return;
    }

    if (success) {
        state_ = FirmwareState::Downloaded;
    } else {
        state_ = FirmwareState::Idle;
        update_result_ = FirmwareUpdateResult::ConnectionLostDuringDownload;
    }
}

void FirmwareUpdateObject::update_complete(FirmwareUpdateResult result) {
    state_ = FirmwareState::Idle;
    update_result_ = result;
    package_data_.clear();

    if (result == FirmwareUpdateResult::Success) {
        package_name_.clear();
        package_version_.clear();
    }
}

std::optional<ResourceType> FirmwareUpdateObject::get_resource_type(
    [[maybe_unused]] InstanceId iid, ResourceId rid) const {
    switch (rid.value) {
        case 0:  // Package
            return ResourceType::Opaque;
        case 1:  // Package URI
        case 6:  // PkgName
        case 7:  // PkgVersion
            return ResourceType::String;
        case 3:  // State
        case 5:  // Update Result
        case 8:  // Protocol Support
        case 9:  // Delivery Method
            return ResourceType::Integer;
        default:
            return std::nullopt;  // Executable (2) has no type
    }
}

} // namespace lwm2m::objects
