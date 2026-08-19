#pragma once

// Ferriot - Strong Types
// Modern C++17 strong typing for LWM2M identifiers

#include <cstdint>
#include <optional>
#include <string>
#include <functional>

namespace lwm2m {

// Strong type wrapper template
template<typename T, typename Tag>
struct StrongType {
    T value;

    constexpr explicit StrongType(T v) noexcept : value(v) {}
    constexpr StrongType() noexcept : value{} {}

    [[nodiscard]] constexpr bool operator==(const StrongType& other) const noexcept {
        return value == other.value;
    }

    [[nodiscard]] constexpr bool operator!=(const StrongType& other) const noexcept {
        return value != other.value;
    }

    [[nodiscard]] constexpr bool operator<(const StrongType& other) const noexcept {
        return value < other.value;
    }
};

// LWM2M identifier types
struct ObjectIdTag {};
struct InstanceIdTag {};
struct ResourceIdTag {};
struct ResourceInstanceIdTag {};

using ObjectId = StrongType<uint16_t, ObjectIdTag>;
using InstanceId = StrongType<uint16_t, InstanceIdTag>;
using ResourceId = StrongType<uint16_t, ResourceIdTag>;
using ResourceInstanceId = StrongType<uint16_t, ResourceInstanceIdTag>;

// Standard object IDs
namespace object_id {
    inline constexpr ObjectId Security{0};
    inline constexpr ObjectId Server{1};
    inline constexpr ObjectId AccessControl{2};
    inline constexpr ObjectId Device{3};
    inline constexpr ObjectId ConnectivityMonitoring{4};
    inline constexpr ObjectId FirmwareUpdate{5};
    inline constexpr ObjectId Location{6};
}

// LWM2M path representation
struct ObjectPath {
    ObjectId object_id;
    std::optional<InstanceId> instance_id;
    std::optional<ResourceId> resource_id;
    std::optional<ResourceInstanceId> resource_instance_id;

    // Construct path from components
    static ObjectPath object(ObjectId oid) {
        return {oid, std::nullopt, std::nullopt, std::nullopt};
    }

    static ObjectPath instance(ObjectId oid, InstanceId iid) {
        return {oid, iid, std::nullopt, std::nullopt};
    }

    static ObjectPath resource(ObjectId oid, InstanceId iid, ResourceId rid) {
        return {oid, iid, rid, std::nullopt};
    }

    // Convert to string representation (e.g., "/3/0/1")
    [[nodiscard]] std::string to_string() const;

    // Parse from string
    [[nodiscard]] static std::optional<ObjectPath> parse(std::string_view path);

    // Check path depth
    [[nodiscard]] bool is_object() const { return !instance_id.has_value(); }
    [[nodiscard]] bool is_instance() const { return instance_id.has_value() && !resource_id.has_value(); }
    [[nodiscard]] bool is_resource() const { return resource_id.has_value() && !resource_instance_id.has_value(); }
    [[nodiscard]] bool is_resource_instance() const { return resource_instance_id.has_value(); }

    [[nodiscard]] bool operator==(const ObjectPath& other) const {
        return object_id == other.object_id
            && instance_id == other.instance_id
            && resource_id == other.resource_id
            && resource_instance_id == other.resource_instance_id;
    }
};

// Resource access permissions
enum class ResourceAccess : uint8_t {
    None    = 0b0000,
    Read    = 0b0001,
    Write   = 0b0010,
    Execute = 0b0100,
    ReadWrite = Read | Write,
};

[[nodiscard]] inline constexpr ResourceAccess operator|(ResourceAccess a, ResourceAccess b) noexcept {
    return static_cast<ResourceAccess>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

[[nodiscard]] inline constexpr bool has_access(ResourceAccess perms, ResourceAccess required) noexcept {
    return (static_cast<uint8_t>(perms) & static_cast<uint8_t>(required)) == static_cast<uint8_t>(required);
}

} // namespace lwm2m

// Hash support for using strong types in unordered containers
namespace std {
template<typename T, typename Tag>
struct hash<lwm2m::StrongType<T, Tag>> {
    size_t operator()(const lwm2m::StrongType<T, Tag>& st) const noexcept {
        return hash<T>{}(st.value);
    }
};
} // namespace std
