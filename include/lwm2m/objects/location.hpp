#pragma once

// Ferriot - Location Object (ID: 6)
// OMA LWM2M Location Object definition

#include "../object.hpp"

#include <chrono>
#include <functional>
#include <optional>

namespace lwm2m::objects {

// GPS position data
struct GpsPosition {
    double latitude = 0.0;      // degrees (-90 to +90)
    double longitude = 0.0;     // degrees (-180 to +180)
    double altitude = 0.0;      // meters above sea level
    double speed = 0.0;         // meters per second
    double radius = 0.0;        // uncertainty radius in meters
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();

    // Validity check
    [[nodiscard]] bool is_valid() const noexcept {
        return latitude >= -90.0 && latitude <= 90.0 &&
               longitude >= -180.0 && longitude <= 180.0;
    }
};

// GPS data provider callback
// Returns nullopt if position is not available
using GpsDataProvider = std::function<std::optional<GpsPosition>()>;

// Location Object (ID: 6) - Single instance
class LocationObject : public SingleInstanceObject<LocationObject, 6> {
public:
    LocationObject();

    // Object interface
    [[nodiscard]] std::string_view name() const noexcept override { return "Location"; }

    // Resource access
    [[nodiscard]] std::vector<ResourceId> list_resources(InstanceId iid) const override;

    [[nodiscard]] Result<ResourceValue> read_resource(
        InstanceId iid,
        ResourceId rid,
        std::optional<ResourceInstanceId> riid
    ) const override;

    [[nodiscard]] Result<void> write_resource(
        InstanceId iid,
        ResourceId rid,
        std::optional<ResourceInstanceId> riid,
        const ResourceValue& value
    ) override;

    [[nodiscard]] Result<void> execute_resource(
        InstanceId iid,
        ResourceId rid,
        std::string_view arguments
    ) override;

    // High-level API - Position management

    // Set a GPS data provider callback (for dynamic position updates)
    void set_gps_provider(GpsDataProvider provider);

    // Update position directly (for manual updates or mock data)
    void update_position(const GpsPosition& position);

    // Get current position
    [[nodiscard]] const GpsPosition& position() const noexcept { return position_; }

    // Check if position data is available
    [[nodiscard]] bool has_position() const noexcept { return has_position_; }

private:
    // Refresh position from provider if available
    void refresh_position() const;

    mutable GpsPosition position_;
    mutable bool has_position_ = false;
    GpsDataProvider gps_provider_;
};

// Resource IDs for Location Object (OMA LWM2M v1.0)
namespace location_resource {
    inline constexpr ResourceId Latitude{0};     // Float, R
    inline constexpr ResourceId Longitude{1};    // Float, R
    inline constexpr ResourceId Altitude{2};     // Float, R
    inline constexpr ResourceId Radius{3};       // Float, R - Uncertainty radius
    inline constexpr ResourceId Velocity{4};     // Opaque, R - 3GPP-TS_23.032 velocity
    inline constexpr ResourceId Timestamp{5};    // Time, R
    inline constexpr ResourceId Speed{6};        // Float, R - Speed in m/s
}

} // namespace lwm2m::objects
