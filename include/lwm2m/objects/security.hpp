#pragma once

// Ferriot - Security Object (ID: 0)
// OMA LWM2M Security Object definition

#include "../object.hpp"
#include "../transport/coap_client.hpp"

#include <string>
#include <vector>

namespace lwm2m::objects {

// Security mode values (Resource 2)
enum class SecurityModeValue : uint8_t {
    PreSharedKey = 0,
    RawPublicKey = 1,
    Certificate = 2,
    NoSec = 3,
    CertificateWithEst = 4,
};

// Security Object instance data
struct SecurityInstance {
    std::string server_uri;              // /0/x/0 - LWM2M Server URI
    bool bootstrap_server = false;       // /0/x/1 - Bootstrap Server
    SecurityModeValue security_mode = SecurityModeValue::NoSec;  // /0/x/2
    std::vector<uint8_t> public_key_identity;   // /0/x/3 - PSK Identity or RPK
    std::vector<uint8_t> server_public_key;     // /0/x/4 - Server Public Key
    std::vector<uint8_t> secret_key;            // /0/x/5 - PSK or Private Key
    uint8_t sms_security_mode = 255;     // /0/x/6 - SMS Security Mode
    std::vector<uint8_t> sms_key_parameters;    // /0/x/7
    std::vector<uint8_t> sms_secret_key;        // /0/x/8
    std::string sms_server_number;       // /0/x/9
    uint16_t short_server_id = 0;        // /0/x/10 - Short Server ID
    uint32_t client_hold_off_time = 0;   // /0/x/11 - Client Hold Off Time
    uint32_t bootstrap_timeout = 0;      // /0/x/12 - Bootstrap Server Account Timeout
};

// Security Object (ID: 0) - Multiple instances
class SecurityObject : public Object {
public:
    SecurityObject();

    // Object interface
    [[nodiscard]] ObjectId id() const noexcept override { return object_id::Security; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Security"; }

    // Instance management
    [[nodiscard]] std::vector<InstanceId> list_instances() const override;
    [[nodiscard]] bool has_instance(InstanceId iid) const override;
    [[nodiscard]] Result<InstanceId> create_instance(std::optional<InstanceId> suggested) override;
    Result<void> delete_instance(InstanceId iid) override;

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

    // High-level API
    Result<InstanceId> add_server(const SecurityInstance& config);
    [[nodiscard]] const SecurityInstance* get_instance(InstanceId iid) const;
    [[nodiscard]] std::optional<SecurityInstance> find_by_short_server_id(uint16_t ssid) const;

    // Convert to transport config
    [[nodiscard]] transport::ConnectionConfig to_connection_config(InstanceId iid) const;

private:
    std::unordered_map<uint16_t, SecurityInstance> instances_;
    uint16_t next_instance_id_ = 0;
};

// Resource IDs for Security Object
namespace security_resource {
    inline constexpr ResourceId ServerUri{0};
    inline constexpr ResourceId BootstrapServer{1};
    inline constexpr ResourceId SecurityMode{2};
    inline constexpr ResourceId PublicKeyIdentity{3};
    inline constexpr ResourceId ServerPublicKey{4};
    inline constexpr ResourceId SecretKey{5};
    inline constexpr ResourceId SmsSecurityMode{6};
    inline constexpr ResourceId SmsKeyParameters{7};
    inline constexpr ResourceId SmsSecretKey{8};
    inline constexpr ResourceId SmsServerNumber{9};
    inline constexpr ResourceId ShortServerId{10};
    inline constexpr ResourceId ClientHoldOffTime{11};
    inline constexpr ResourceId BootstrapTimeout{12};
}

} // namespace lwm2m::objects
