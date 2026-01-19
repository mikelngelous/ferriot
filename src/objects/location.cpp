// Ferriot - Location Object Implementation

#include "lwm2m/objects/location.hpp"

namespace lwm2m::objects {

LocationObject::LocationObject() {
    // Initialize with default invalid position
    position_.latitude = 0.0;
    position_.longitude = 0.0;
    position_.altitude = 0.0;
    position_.speed = 0.0;
    position_.radius = 0.0;
    position_.timestamp = std::chrono::system_clock::now();
    has_position_ = false;
}

std::vector<ResourceId> LocationObject::list_resources(InstanceId /* iid */) const {
    return {
        location_resource::Latitude,
        location_resource::Longitude,
        location_resource::Altitude,
        location_resource::Radius,
        location_resource::Timestamp,
        location_resource::Speed,
    };
}

Result<ResourceValue> LocationObject::read_resource(
    InstanceId iid,
    ResourceId rid,
    [[maybe_unused]] std::optional<ResourceInstanceId> riid
) const {
    if (iid.value != 0) {
        return Err<ResourceValue>(ErrorCode::NotFound, "Instance not found");
    }

    refresh_position();

    if (!has_position_) {
        return Err<ResourceValue>(ErrorCode::NotFound, "Position not available");
    }

    switch (rid.value) {
        case 0:  // Latitude
            return Ok<ResourceValue>(position_.latitude);
        case 1:  // Longitude
            return Ok<ResourceValue>(position_.longitude);
        case 2:  // Altitude
            return Ok<ResourceValue>(position_.altitude);
        case 3:  // Radius (uncertainty)
            return Ok<ResourceValue>(position_.radius);
        case 4:  // Velocity (Opaque - not implemented)
            return Err<ResourceValue>(ErrorCode::NotFound, "Velocity not implemented");
        case 5:  // Timestamp
            return Ok<ResourceValue>(position_.timestamp);
        case 6:  // Speed
            return Ok<ResourceValue>(position_.speed);
        default:
            return Err<ResourceValue>(ErrorCode::NotFound, "Resource not found");
    }
}

Result<void> LocationObject::write_resource(
    InstanceId iid,
    ResourceId /* rid */,
    [[maybe_unused]] std::optional<ResourceInstanceId> riid,
    [[maybe_unused]] const ResourceValue& value
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    // All resources in Location Object are read-only
    return Err<void>(ErrorCode::MethodNotAllowed, "Location resources are read-only");
}

Result<void> LocationObject::execute_resource(
    InstanceId iid,
    ResourceId /* rid */,
    [[maybe_unused]] std::string_view arguments
) {
    if (iid.value != 0) {
        return Err<void>(ErrorCode::NotFound, "Instance not found");
    }

    // No executable resources in Location Object
    return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not executable");
}

void LocationObject::set_gps_provider(GpsDataProvider provider) {
    gps_provider_ = std::move(provider);
}

void LocationObject::update_position(const GpsPosition& position) {
    position_ = position;
    has_position_ = position.is_valid();
}

void LocationObject::refresh_position() const {
    if (gps_provider_) {
        if (auto pos = gps_provider_()) {
            position_ = *pos;
            has_position_ = pos->is_valid();
        }
    }
}

} // namespace lwm2m::objects
