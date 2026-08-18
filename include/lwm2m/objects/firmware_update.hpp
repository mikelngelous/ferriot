#pragma once

// Ferriot - Firmware Update Object (ID: 5)
// OMA LWM2M Firmware Update Object definition

#include "../object.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace lwm2m::objects {

// Firmware Update states
enum class FirmwareState : uint8_t {
    Idle = 0,
    Downloading = 1,
    Downloaded = 2,
    Updating = 3,
};

// Firmware Update results
enum class FirmwareUpdateResult : uint8_t {
    Initial = 0,
    Success = 1,
    NotEnoughFlashMemory = 2,
    OutOfRam = 3,
    ConnectionLostDuringDownload = 4,
    IntegrityCheckFailure = 5,
    UnsupportedPackageType = 6,
    InvalidUri = 7,
    UpdateFailed = 8,
    UnsupportedProtocol = 9,
};

// Firmware Update protocols
enum class FirmwareProtocol : uint8_t {
    CoAP = 0,
    CoAPS = 1,
    HTTP = 2,
    HTTPS = 3,
    CoAPTCP = 4,
    CoAPSTCP = 5,
};

// Firmware Update delivery methods
enum class FirmwareDeliveryMethod : uint8_t {
    Pull = 0,    // Device downloads from URI
    Push = 1,    // Server pushes package
    Both = 2,    // Both methods supported
};

// Callbacks for firmware update operations
struct FirmwareUpdateCallbacks {
    // Called when download from URI is requested
    // Should return true if download started successfully
    std::function<Result<void>(const std::string& uri)> start_download;

    // Called to verify downloaded package
    // Returns true if package is valid
    std::function<Result<void>(const std::vector<uint8_t>& package)> verify_package;

    // Called to apply the firmware update
    // Device should reboot after successful update
    std::function<Result<void>()> apply_update;

    // Called when update is cancelled
    std::function<void()> on_cancel;
};

// Firmware Update Object (ID: 5) - Single instance
class FirmwareUpdateObject : public SingleInstanceObject<FirmwareUpdateObject, 5> {
public:
    FirmwareUpdateObject();

    // Object interface
    [[nodiscard]] std::string_view name() const noexcept override { return "Firmware Update"; }

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

    [[nodiscard]] std::optional<ResourceType> get_resource_type(
        InstanceId iid, ResourceId rid) const override;

    // High-level API

    void set_callbacks(FirmwareUpdateCallbacks callbacks);

    // State management
    [[nodiscard]] FirmwareState state() const noexcept { return state_.load(); }
    void set_state(FirmwareState state);

    [[nodiscard]] FirmwareUpdateResult update_result() const noexcept { return update_result_.load(); }
    void set_update_result(FirmwareUpdateResult result);

    // Package info (set after download/verification)
    void set_package_info(const std::string& name, const std::string& version);

    void set_supported_protocols(std::vector<FirmwareProtocol> protocols);

    void set_delivery_method(FirmwareDeliveryMethod method);

    // Push package data (for server-initiated push)
    Result<void> push_package(const std::vector<uint8_t>& data);

    // Called by application when download completes
    void download_complete(bool success);

    // Called by application when update completes
    void update_complete(FirmwareUpdateResult result);

private:
    // Written by the download thread, read by the CoAP thread
    std::atomic<FirmwareState> state_{FirmwareState::Idle};
    std::atomic<FirmwareUpdateResult> update_result_{FirmwareUpdateResult::Initial};

    std::string package_uri_;
    std::vector<uint8_t> package_data_;
    std::string package_name_;
    std::string package_version_;

    std::vector<FirmwareProtocol> supported_protocols_ = {
        FirmwareProtocol::CoAP,
        FirmwareProtocol::CoAPS,
        FirmwareProtocol::HTTP,
        FirmwareProtocol::HTTPS,
    };
    FirmwareDeliveryMethod delivery_method_ = FirmwareDeliveryMethod::Both;

    FirmwareUpdateCallbacks callbacks_;
};

// Resource IDs for Firmware Update Object (OMA LWM2M v1.0)
namespace firmware_resource {
    inline constexpr ResourceId Package{0};                  // Opaque, W
    inline constexpr ResourceId PackageUri{1};               // String, W
    inline constexpr ResourceId Update{2};                   // Executable
    inline constexpr ResourceId State{3};                    // Integer, R
    inline constexpr ResourceId UpdateSupportedObjects{4};   // Boolean, RW (deprecated in 1.1)
    inline constexpr ResourceId UpdateResult{5};             // Integer, R
    inline constexpr ResourceId PkgName{6};                  // String, R
    inline constexpr ResourceId PkgVersion{7};               // String, R
    inline constexpr ResourceId ProtocolSupport{8};          // Integer, R, Multiple
    inline constexpr ResourceId DeliveryMethod{9};           // Integer, R
}

} // namespace lwm2m::objects
