// Ferriot - Device Object Implementation

#include "lwm2m/objects/device.hpp"

namespace lwm2m::objects {

DeviceObject::DeviceObject(DeviceConfig config)
    : config_(std::move(config)) {}

std::vector<ResourceId> DeviceObject::list_resources(InstanceId /* iid */) const {
    return {
        device_resource::Manufacturer,
        device_resource::ModelNumber,
        device_resource::SerialNumber,
        device_resource::FirmwareVersion,
        device_resource::Reboot,
        device_resource::FactoryReset,
        device_resource::AvailablePowerSources,
        device_resource::PowerSourceVoltage,
        device_resource::PowerSourceCurrent,
        device_resource::BatteryLevel,
        device_resource::MemoryFree,
        device_resource::ErrorCode,
        device_resource::ResetErrorCode,
        device_resource::CurrentTime,
        device_resource::UtcOffset,
        device_resource::Timezone,
        device_resource::SupportedBindings,
        device_resource::DeviceType,
        device_resource::HardwareVersion,
        device_resource::SoftwareVersion,
        device_resource::BatteryStatus,
        device_resource::MemoryTotal,
    };
}

Result<ResourceValue> DeviceObject::read_resource(
    InstanceId iid,
    ResourceId rid,
    std::optional<ResourceInstanceId> riid
) const {
    if (iid.value != 0) {
        return Err<ResourceValue>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 0:  // Manufacturer
            return Ok<ResourceValue>(config_.manufacturer);
        case 1:  // Model Number
            return Ok<ResourceValue>(config_.model_number);
        case 2:  // Serial Number
            return Ok<ResourceValue>(config_.serial_number);
        case 3:  // Firmware Version
            return Ok<ResourceValue>(config_.firmware_version);
        case 6:  // Available Power Sources (multiple)
            if (riid.has_value()) {
                if (riid->value < config_.available_power_sources.size()) {
                    return Ok<ResourceValue>(
                        static_cast<int64_t>(config_.available_power_sources[riid->value])
                    );
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            // Return first instance if no riid specified
            if (!config_.available_power_sources.empty()) {
                return Ok<ResourceValue>(
                    static_cast<int64_t>(config_.available_power_sources[0])
                );
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No power sources");
        case 7:  // Power Source Voltage (multiple)
            if (riid.has_value()) {
                if (riid->value < config_.power_source_voltages.size()) {
                    return Ok<ResourceValue>(
                        static_cast<int64_t>(config_.power_source_voltages[riid->value])
                    );
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!config_.power_source_voltages.empty()) {
                return Ok<ResourceValue>(
                    static_cast<int64_t>(config_.power_source_voltages[0])
                );
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No voltage data");
        case 8:  // Power Source Current (multiple)
            if (riid.has_value()) {
                if (riid->value < config_.power_source_currents.size()) {
                    return Ok<ResourceValue>(
                        static_cast<int64_t>(config_.power_source_currents[riid->value])
                    );
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!config_.power_source_currents.empty()) {
                return Ok<ResourceValue>(
                    static_cast<int64_t>(config_.power_source_currents[0])
                );
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No current data");
        case 9:  // Battery Level
            return Ok<ResourceValue>(static_cast<int64_t>(config_.battery_level));
        case 10: // Memory Free
            return Ok<ResourceValue>(static_cast<int64_t>(config_.memory_free));
        case 11: // Error Code (multiple)
            if (riid.has_value()) {
                if (riid->value < error_codes_.size()) {
                    return Ok<ResourceValue>(
                        static_cast<int64_t>(error_codes_[riid->value])
                    );
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!error_codes_.empty()) {
                return Ok<ResourceValue>(static_cast<int64_t>(error_codes_[0]));
            }
            return Ok<ResourceValue>(static_cast<int64_t>(DeviceErrorCode::NoError));
        case 13: // Current Time
            return Ok<ResourceValue>(current_time());
        case 14: // UTC Offset
            return Ok<ResourceValue>(config_.utc_offset);
        case 15: // Timezone
            return Ok<ResourceValue>(config_.timezone);
        case 16: // Supported Binding and Modes
            return Ok<ResourceValue>(config_.supported_bindings);
        case 17: // Device Type
            return Ok<ResourceValue>(config_.device_type);
        case 18: // Hardware Version
            return Ok<ResourceValue>(config_.hardware_version);
        case 19: // Software Version
            return Ok<ResourceValue>(config_.software_version);
        case 20: // Battery Status
            return Ok<ResourceValue>(static_cast<int64_t>(config_.battery_status));
        case 21: // Memory Total
            return Ok<ResourceValue>(static_cast<int64_t>(config_.memory_total));
        default:
            return Err<ResourceValue>(ErrorCode::NotFound, "Resource not found");
    }
}

Result<void> DeviceObject::write_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::optional<ResourceInstanceId> riid,
    const ResourceValue& value
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 13: // Current Time
            // Time is read-only in most implementations
            return Err<void>(ErrorCode::MethodNotAllowed, "Current Time is read-only");
        case 14: // UTC Offset
            if (auto* str = std::get_if<std::string>(&value)) {
                config_.utc_offset = *str;
                return Ok();
            }
            return Err<void>(ErrorCode::BadRequest, "Invalid value type");
        case 15: // Timezone
            if (auto* str = std::get_if<std::string>(&value)) {
                config_.timezone = *str;
                return Ok();
            }
            return Err<void>(ErrorCode::BadRequest, "Invalid value type");
        default:
            return Err<void>(ErrorCode::MethodNotAllowed, "Resource is read-only");
    }
}

Result<void> DeviceObject::execute_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::string_view arguments
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 4:  // Reboot
            if (reboot_callback_) {
                reboot_callback_();
            }
            return Ok();
        case 5:  // Factory Reset
            if (factory_reset_callback_) {
                factory_reset_callback_();
            }
            return Ok();
        case 12: // Reset Error Code
            clear_error_codes();
            return Ok();
        default:
            return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not executable");
    }
}

void DeviceObject::set_battery_level(uint8_t level) {
    config_.battery_level = level;
}

void DeviceObject::set_battery_status(BatteryStatus status) {
    config_.battery_status = status;
}

void DeviceObject::set_memory_free(int32_t kb) {
    config_.memory_free = kb;
}

void DeviceObject::add_error_code(DeviceErrorCode code) {
    // Remove NoError if we're adding a real error
    if (code != DeviceErrorCode::NoError && !error_codes_.empty() &&
        error_codes_[0] == DeviceErrorCode::NoError) {
        error_codes_.clear();
    }
    error_codes_.push_back(code);
}

void DeviceObject::clear_error_codes() {
    error_codes_.clear();
    error_codes_.push_back(DeviceErrorCode::NoError);
}

void DeviceObject::set_reboot_callback(RebootCallback callback) {
    reboot_callback_ = std::move(callback);
}

void DeviceObject::set_factory_reset_callback(FactoryResetCallback callback) {
    factory_reset_callback_ = std::move(callback);
}

std::chrono::system_clock::time_point DeviceObject::current_time() const {
    return std::chrono::system_clock::now();
}

} // namespace lwm2m::objects
