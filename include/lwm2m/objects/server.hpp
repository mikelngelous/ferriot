#pragma once

// Ferriot - Server Object (ID: 1)
// OMA LWM2M Server Object definition

#include "../object.hpp"

#include <chrono>
#include <string>

namespace lwm2m::objects {

// Binding mode values
enum class BindingMode : uint8_t {
    Udp = 'U',
    UdpQueueMode = 'Q',
    Sms = 'S',
    UdpSms = 'M',
    UdpQueueSms = 'N',
    Tcp = 'T',
    TcpQueueMode = 'P',
};

// Server Object instance data
struct ServerInstance {
    uint16_t short_server_id = 0;        // /1/x/0 - Short Server ID
    uint32_t lifetime = 86400;           // /1/x/1 - Lifetime (seconds)
    uint32_t default_min_period = 0;     // /1/x/2 - Default Minimum Period
    uint32_t default_max_period = 0;     // /1/x/3 - Default Maximum Period
    bool disable_timeout_present = false;
    uint32_t disable_timeout = 86400;    // /1/x/5 - Disable Timeout
    bool notification_storing = false;   // /1/x/6 - Notification Storing
    std::string binding = "U";           // /1/x/7 - Binding
    // /1/x/8 - Registration Update Trigger (executable)
    // /1/x/9 - Bootstrap Request Trigger (executable) - LWM2M 1.1
    // Additional LWM2M 1.1+ resources...
};

// Server Object (ID: 1) - Multiple instances
class ServerObject : public Object {
public:
    ServerObject();

    // Object interface
    [[nodiscard]] ObjectId id() const noexcept override { return object_id::Server; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Server"; }

    // Instance management
    [[nodiscard]] std::vector<InstanceId> list_instances() const override;
    [[nodiscard]] bool has_instance(InstanceId iid) const override;
    [[nodiscard]] Result<InstanceId> create_instance(std::optional<InstanceId> suggested) override;
    Result<void> delete_instance(InstanceId iid) override;

    // Resource access
    [[nodiscard]] std::vector<ResourceId> list_resources(InstanceId iid) const override;

    [[nodiscard]] std::optional<ResourceType> get_resource_type(
        InstanceId iid, ResourceId rid) const override;

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

    // High-level API
    Result<InstanceId> add_server(const ServerInstance& config);
    [[nodiscard]] const ServerInstance* get_instance(InstanceId iid) const;
    [[nodiscard]] std::optional<ServerInstance> find_by_short_server_id(uint16_t ssid) const;

    // Callbacks for executable resources
    using RegistrationUpdateCallback = std::function<void(uint16_t short_server_id)>;
    void set_registration_update_callback(RegistrationUpdateCallback callback);

private:
    std::unordered_map<uint16_t, ServerInstance> instances_;
    uint16_t next_instance_id_ = 0;
    RegistrationUpdateCallback registration_update_callback_;
};

// Resource IDs for Server Object
namespace server_resource {
    inline constexpr ResourceId ShortServerId{0};
    inline constexpr ResourceId Lifetime{1};
    inline constexpr ResourceId DefaultMinPeriod{2};
    inline constexpr ResourceId DefaultMaxPeriod{3};
    inline constexpr ResourceId Disable{4};  // Executable
    inline constexpr ResourceId DisableTimeout{5};
    inline constexpr ResourceId NotificationStoring{6};
    inline constexpr ResourceId Binding{7};
    inline constexpr ResourceId RegistrationUpdateTrigger{8};  // Executable
    inline constexpr ResourceId BootstrapRequestTrigger{9};    // Executable (LWM2M 1.1)
}

} // namespace lwm2m::objects
