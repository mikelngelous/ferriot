#pragma once

// Ferriot - Connectivity Monitoring Object (ID: 4)
// OMA LWM2M Connectivity Monitoring Object definition

#include "../object.hpp"

#include <string>
#include <vector>

namespace lwm2m::objects {

// Network bearer types (OMA LWM2M 1.0)
enum class NetworkBearer : uint8_t {
    GSM = 0,
    TDSCDMA = 1,
    WCDMA = 2,
    CDMA2000 = 3,
    WiMAX = 4,
    LTE = 5,
    NR5G = 6,               // 5G New Radio
    Reserved7 = 7,
    Reserved8 = 8,
    Reserved9 = 9,
    Reserved10 = 10,
    Reserved11 = 11,
    Reserved12 = 12,
    Reserved13 = 13,
    Reserved14 = 14,
    Reserved15 = 15,
    Reserved16 = 16,
    Reserved17 = 17,
    Reserved18 = 18,
    Reserved19 = 19,
    Reserved20 = 20,
    WiFi = 21,
    Bluetooth = 22,
    IEEE802p15p4 = 23,      // 802.15.4
    Reserved24 = 24,
    Reserved25 = 25,
    Reserved26 = 26,
    Reserved27 = 27,
    Reserved28 = 28,
    Reserved29 = 29,
    Reserved30 = 30,
    Reserved31 = 31,
    Reserved32 = 32,
    Reserved33 = 33,
    Reserved34 = 34,
    Reserved35 = 35,
    Reserved36 = 36,
    Reserved37 = 37,
    Reserved38 = 38,
    Reserved39 = 39,
    Reserved40 = 40,
    Ethernet = 41,
    DSL = 42,
    PLC = 43,               // Power Line Communication
    CV2X = 44,              // Cellular V2X
    CV2X_DSRC = 45,         // C-V2X DSRC
};

// Connectivity Monitoring configuration
struct ConnectivityConfig {
    // Current network bearer (Resource 0)
    NetworkBearer network_bearer = NetworkBearer::Ethernet;

    // Available network bearers (Resource 1, multiple)
    std::vector<NetworkBearer> available_bearers = {NetworkBearer::Ethernet};

    // Radio Signal Strength in dBm (Resource 2)
    // Range: typically -120 to -40 dBm for cellular
    int32_t radio_signal_strength = -70;

    // Link Quality 0-100% (Resource 3)
    uint8_t link_quality = 100;

    // IP Addresses (Resource 4, multiple)
    std::vector<std::string> ip_addresses;

    // Router IP Addresses (Resource 5, multiple)
    std::vector<std::string> router_ip_addresses;

    // Link Utilization 0-100% (Resource 6)
    uint8_t link_utilization = 0;

    // APN list (Resource 7, multiple) - for cellular only
    std::vector<std::string> apn_list;

    // Cell ID (Resource 8) - for cellular only
    uint32_t cell_id = 0;

    // Serving Mobile Network Code (Resource 9)
    uint16_t smnc = 0;

    // Serving Mobile Country Code (Resource 10)
    uint16_t smcc = 0;
};

// Connectivity Monitoring Object (ID: 4) - Single instance
class ConnectivityObject : public SingleInstanceObject<ConnectivityObject, 4> {
public:
    explicit ConnectivityObject(ConnectivityConfig config = {});

    // Object interface
    [[nodiscard]] std::string_view name() const noexcept override { return "Connectivity Monitoring"; }

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

    // High-level API - Update connectivity state

    void set_network_bearer(NetworkBearer bearer);

    void set_available_bearers(std::vector<NetworkBearer> bearers);

    void set_signal_strength(int32_t dbm);

    void set_link_quality(uint8_t percent);

    void set_ip_addresses(std::vector<std::string> addresses);

    void set_router_ip_addresses(std::vector<std::string> addresses);

    void set_link_utilization(uint8_t percent);

    void set_cellular_info(uint32_t cell_id, uint16_t mcc, uint16_t mnc);

    void set_apn_list(std::vector<std::string> apns);

    [[nodiscard]] const ConnectivityConfig& config() const noexcept { return config_; }

private:
    ConnectivityConfig config_;
};

// Resource IDs for Connectivity Monitoring Object (OMA LWM2M v1.0)
namespace connectivity_resource {
    inline constexpr ResourceId NetworkBearer{0};              // Integer, R
    inline constexpr ResourceId AvailableNetworkBearer{1};     // Integer, R, Multiple
    inline constexpr ResourceId RadioSignalStrength{2};        // Integer, R
    inline constexpr ResourceId LinkQuality{3};                // Integer, R
    inline constexpr ResourceId IpAddresses{4};                // String, R, Multiple
    inline constexpr ResourceId RouterIpAddresses{5};          // String, R, Multiple
    inline constexpr ResourceId LinkUtilization{6};            // Integer, R
    inline constexpr ResourceId Apn{7};                        // String, R, Multiple
    inline constexpr ResourceId CellId{8};                     // Integer, R
    inline constexpr ResourceId Smnc{9};                       // Integer, R
    inline constexpr ResourceId Smcc{10};                      // Integer, R
}

} // namespace lwm2m::objects
