#pragma once

// Ferriot - Resource Types
// Type-safe resource values and handlers

#include "types.hpp"
#include "result.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace lwm2m {

// Resource value variant - all possible LWM2M resource types
using ResourceValue = std::variant<
    std::monostate,                           // null/empty
    bool,                                     // Boolean
    int64_t,                                  // Integer
    uint64_t,                                 // Unsigned Integer
    double,                                   // Float
    std::string,                              // String
    std::vector<uint8_t>,                     // Opaque (binary)
    std::chrono::system_clock::time_point,    // Time
    ObjectPath                                // Object Link
>;

// Resource type enum (for serialization)
enum class ResourceType {
    None,
    Boolean,
    Integer,
    UnsignedInteger,
    Float,
    String,
    Opaque,
    Time,
    ObjectLink,
};

// Get type enum from variant
[[nodiscard]] ResourceType get_resource_type(const ResourceValue& value);

// Resource definition (metadata)
struct ResourceDefinition {
    ResourceId id;
    std::string name;
    ResourceType type;
    ResourceAccess access;
    bool multiple_instances;
    bool mandatory;
};

// Callback types for resource handlers
using ReadHandler = std::function<Result<ResourceValue>(
    InstanceId instance_id,
    std::optional<ResourceInstanceId> resource_instance_id
)>;

using WriteHandler = std::function<Result<void>(
    InstanceId instance_id,
    std::optional<ResourceInstanceId> resource_instance_id,
    const ResourceValue& value
)>;

using ExecuteHandler = std::function<Result<void>(
    InstanceId instance_id,
    std::string_view arguments
)>;

// Resource with handlers (fluent builder pattern)
class Resource {
public:
    explicit Resource(ResourceDefinition def);

    // Fluent API for setting handlers
    Resource& on_read(ReadHandler handler);
    Resource& on_write(WriteHandler handler);
    Resource& on_execute(ExecuteHandler handler);

    // Accessors
    [[nodiscard]] const ResourceDefinition& definition() const noexcept { return def_; }
    [[nodiscard]] ResourceId id() const noexcept { return def_.id; }
    [[nodiscard]] std::string_view name() const noexcept { return def_.name; }
    [[nodiscard]] ResourceType type() const noexcept { return def_.type; }
    [[nodiscard]] ResourceAccess access() const noexcept { return def_.access; }

    // Operations
    [[nodiscard]] Result<ResourceValue> read(
        InstanceId instance_id,
        std::optional<ResourceInstanceId> resource_instance_id = std::nullopt
    ) const;

    [[nodiscard]] Result<void> write(
        InstanceId instance_id,
        std::optional<ResourceInstanceId> resource_instance_id,
        const ResourceValue& value
    );

    [[nodiscard]] Result<void> execute(
        InstanceId instance_id,
        std::string_view arguments
    );

private:
    ResourceDefinition def_;
    ReadHandler read_handler_;
    WriteHandler write_handler_;
    ExecuteHandler execute_handler_;
};

} // namespace lwm2m
