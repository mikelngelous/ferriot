#pragma once

// Ferriot - Device Object (ID: 3)
// OMA LWM2M Device Object definition

#include "../object.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace lwm2m::objects {

// Error codes for Device Object
enum class DeviceErrorCode : uint8_t {
    NoError = 0,
    LowBatteryPower = 1,
    ExternalPowerSupplyOff = 2,
    GpsModuleFailure = 3,
    LowReceivedSignalStrength = 4,
    OutOfMemory = 5,
    SmsFailure = 6,
    IpConnectivityFailure = 7,
    PeripheralMalfunction = 8,
};

// Battery status values
enum class BatteryStatus : uint8_t {
    Normal = 0,
    Charging = 1,
    ChargeComplete = 2,
    Damaged = 3,
    LowBattery = 4,
    NotInstalled = 5,
    Unknown = 6,
};

// Device configuration (set at construction)
struct DeviceConfig {
    std::string manufacturer = "Ferriot";
    std::string model_number = "Ferriot";
    std::string serial_number;
    std::string firmware_version = "1.0.0";
    std::string software_version = "1.0.0";
    std::string hardware_version = "1.0";
    std::string device_type = "TCU";

    // Power info (can be updated at runtime)
    std::vector<uint8_t> available_power_sources = {0};  // 0=DC, 1=Internal Battery
    std::vector<int32_t> power_source_voltages = {12000};  // mV
    std::vector<int32_t> power_source_currents = {0};      // mA
    uint8_t battery_level = 100;                           // 0-100%
    BatteryStatus battery_status = BatteryStatus::Normal;

    // Memory info
    int32_t memory_free = 0;        // KB
    int32_t memory_total = 0;       // KB (read from /0/x/21)

    // Timezone and UTC offset
    std::string timezone;           // IANA timezone string
    std::string utc_offset;         // e.g., "+02:00"

    // Device binding and modes
    std::string supported_bindings = "U";  // UDP
};

// Device Object (ID: 3) - Single instance
class DeviceObject : public SingleInstanceObject<DeviceObject, 3> {
public:
    explicit DeviceObject(DeviceConfig config = {});

    // Object interface
    [[nodiscard]] std::string_view name() const noexcept override { return "Device"; }

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

    // High-level API - Update device state
    void set_battery_level(uint8_t level);
    void set_battery_status(BatteryStatus status);
    void set_memory_free(int32_t kb);
    void add_error_code(DeviceErrorCode code);
    void clear_error_codes();

    // Callbacks for executable resources
    using RebootCallback = std::function<void()>;
    using FactoryResetCallback = std::function<void()>;
    using ResetErrorCodeCallback = std::function<void()>;

    void set_reboot_callback(RebootCallback callback);
    void set_factory_reset_callback(FactoryResetCallback callback);

    // Get current time (for /3/0/13)
    [[nodiscard]] std::chrono::system_clock::time_point current_time() const;

private:
    DeviceConfig config_;
    std::vector<DeviceErrorCode> error_codes_ = {DeviceErrorCode::NoError};

    RebootCallback reboot_callback_;
    FactoryResetCallback factory_reset_callback_;
};

// Resource IDs for Device Object
namespace device_resource {
    inline constexpr ResourceId Manufacturer{0};
    inline constexpr ResourceId ModelNumber{1};
    inline constexpr ResourceId SerialNumber{2};
    inline constexpr ResourceId FirmwareVersion{3};
    inline constexpr ResourceId Reboot{4};              // Executable
    inline constexpr ResourceId FactoryReset{5};        // Executable
    inline constexpr ResourceId AvailablePowerSources{6};  // Multiple
    inline constexpr ResourceId PowerSourceVoltage{7};     // Multiple
    inline constexpr ResourceId PowerSourceCurrent{8};     // Multiple
    inline constexpr ResourceId BatteryLevel{9};
    inline constexpr ResourceId MemoryFree{10};
    inline constexpr ResourceId ErrorCode{11};          // Multiple
    inline constexpr ResourceId ResetErrorCode{12};     // Executable
    inline constexpr ResourceId CurrentTime{13};
    inline constexpr ResourceId UtcOffset{14};
    inline constexpr ResourceId Timezone{15};
    inline constexpr ResourceId SupportedBindings{16};
    inline constexpr ResourceId DeviceType{17};
    inline constexpr ResourceId HardwareVersion{18};
    inline constexpr ResourceId SoftwareVersion{19};
    inline constexpr ResourceId BatteryStatus{20};
    inline constexpr ResourceId MemoryTotal{21};
}

} // namespace lwm2m::objects
