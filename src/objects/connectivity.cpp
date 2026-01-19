// Ferriot - Connectivity Monitoring Object Implementation

#include "lwm2m/objects/connectivity.hpp"

namespace lwm2m::objects {

ConnectivityObject::ConnectivityObject(ConnectivityConfig config)
    : config_(std::move(config)) {}

std::vector<ResourceId> ConnectivityObject::list_resources(InstanceId /* iid */) const {
    return {
        connectivity_resource::NetworkBearer,
        connectivity_resource::AvailableNetworkBearer,
        connectivity_resource::RadioSignalStrength,
        connectivity_resource::LinkQuality,
        connectivity_resource::IpAddresses,
        connectivity_resource::RouterIpAddresses,
        connectivity_resource::LinkUtilization,
        connectivity_resource::Apn,
        connectivity_resource::CellId,
        connectivity_resource::Smnc,
        connectivity_resource::Smcc,
    };
}

Result<ResourceValue> ConnectivityObject::read_resource(
    InstanceId iid,
    ResourceId rid,
    std::optional<ResourceInstanceId> riid
) const {
    if (iid.value != 0) {
        return Err<ResourceValue>(ErrorCode::NotFound, "Instance not found");
    }

    switch (rid.value) {
        case 0:  // Network Bearer
            return Ok<ResourceValue>(static_cast<int64_t>(config_.network_bearer));

        case 1:  // Available Network Bearers (multiple)
            if (riid.has_value()) {
                if (riid->value < config_.available_bearers.size()) {
                    return Ok<ResourceValue>(
                        static_cast<int64_t>(config_.available_bearers[riid->value])
                    );
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!config_.available_bearers.empty()) {
                return Ok<ResourceValue>(
                    static_cast<int64_t>(config_.available_bearers[0])
                );
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No bearers available");

        case 2:  // Radio Signal Strength
            return Ok<ResourceValue>(static_cast<int64_t>(config_.radio_signal_strength));

        case 3:  // Link Quality
            return Ok<ResourceValue>(static_cast<int64_t>(config_.link_quality));

        case 4:  // IP Addresses (multiple)
            if (riid.has_value()) {
                if (riid->value < config_.ip_addresses.size()) {
                    return Ok<ResourceValue>(config_.ip_addresses[riid->value]);
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!config_.ip_addresses.empty()) {
                return Ok<ResourceValue>(config_.ip_addresses[0]);
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No IP addresses");

        case 5:  // Router IP Addresses (multiple)
            if (riid.has_value()) {
                if (riid->value < config_.router_ip_addresses.size()) {
                    return Ok<ResourceValue>(config_.router_ip_addresses[riid->value]);
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!config_.router_ip_addresses.empty()) {
                return Ok<ResourceValue>(config_.router_ip_addresses[0]);
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No router IP addresses");

        case 6:  // Link Utilization
            return Ok<ResourceValue>(static_cast<int64_t>(config_.link_utilization));

        case 7:  // APN (multiple)
            if (riid.has_value()) {
                if (riid->value < config_.apn_list.size()) {
                    return Ok<ResourceValue>(config_.apn_list[riid->value]);
                }
                return Err<ResourceValue>(ErrorCode::NotFound, "Resource instance not found");
            }
            if (!config_.apn_list.empty()) {
                return Ok<ResourceValue>(config_.apn_list[0]);
            }
            return Err<ResourceValue>(ErrorCode::NotFound, "No APNs configured");

        case 8:  // Cell ID
            return Ok<ResourceValue>(static_cast<int64_t>(config_.cell_id));

        case 9:  // SMNC
            return Ok<ResourceValue>(static_cast<int64_t>(config_.smnc));

        case 10: // SMCC
            return Ok<ResourceValue>(static_cast<int64_t>(config_.smcc));

        default:
            return Err<ResourceValue>(ErrorCode::NotFound, "Resource not found");
    }
}

Result<void> ConnectivityObject::write_resource(
    InstanceId iid,
    ResourceId /* rid */,
    [[maybe_unused]] std::optional<ResourceInstanceId> riid,
    [[maybe_unused]] const ResourceValue& value
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    // All resources in Connectivity Monitoring Object are read-only
    return Err<void>(ErrorCode::MethodNotAllowed, "Connectivity resources are read-only");
}

Result<void> ConnectivityObject::execute_resource(
    InstanceId iid,
    ResourceId /* rid */,
    [[maybe_unused]] std::string_view arguments
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    // No executable resources in Connectivity Monitoring Object
    return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not executable");
}

void ConnectivityObject::set_network_bearer(NetworkBearer bearer) {
    config_.network_bearer = bearer;
}

void ConnectivityObject::set_available_bearers(std::vector<NetworkBearer> bearers) {
    config_.available_bearers = std::move(bearers);
}

void ConnectivityObject::set_signal_strength(int32_t dbm) {
    config_.radio_signal_strength = dbm;
}

void ConnectivityObject::set_link_quality(uint8_t percent) {
    config_.link_quality = percent > 100 ? 100 : percent;
}

void ConnectivityObject::set_ip_addresses(std::vector<std::string> addresses) {
    config_.ip_addresses = std::move(addresses);
}

void ConnectivityObject::set_router_ip_addresses(std::vector<std::string> addresses) {
    config_.router_ip_addresses = std::move(addresses);
}

void ConnectivityObject::set_link_utilization(uint8_t percent) {
    config_.link_utilization = percent > 100 ? 100 : percent;
}

void ConnectivityObject::set_cellular_info(uint32_t cell_id, uint16_t mcc, uint16_t mnc) {
    config_.cell_id = cell_id;
    config_.smcc = mcc;
    config_.smnc = mnc;
}

void ConnectivityObject::set_apn_list(std::vector<std::string> apns) {
    config_.apn_list = std::move(apns);
}

std::optional<ResourceType> ConnectivityObject::get_resource_type(
    [[maybe_unused]] InstanceId iid, ResourceId rid) const {
    switch (rid.value) {
        case 0:  // Network Bearer
        case 1:  // Available Network Bearer
        case 2:  // Radio Signal Strength
        case 3:  // Link Quality
        case 6:  // Link Utilization
        case 8:  // Cell ID
        case 9:  // SMNC
        case 10: // SMCC
            return ResourceType::Integer;
        case 4:  // IP Addresses
        case 5:  // Router IP Addresses
        case 7:  // APN
            return ResourceType::String;
        default:
            return std::nullopt;
    }
}

} // namespace lwm2m::objects
