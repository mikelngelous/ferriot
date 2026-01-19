#pragma once

// Ferriot - Object Base Class
// Abstract interface for LWM2M objects

#include "types.hpp"
#include "result.hpp"
#include "resource.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace lwm2m {

// Abstract base class for all LWM2M objects
class Object {
public:
    virtual ~Object() = default;

    // Object identification
    [[nodiscard]] virtual ObjectId id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    // Instance management
    [[nodiscard]] virtual std::vector<InstanceId> list_instances() const = 0;
    [[nodiscard]] virtual bool has_instance(InstanceId iid) const = 0;

    // Optional: create/delete instances (for multiple-instance objects)
    [[nodiscard]] virtual Result<InstanceId> create_instance(
        std::optional<InstanceId> suggested = std::nullopt
    );
    virtual Result<void> delete_instance(InstanceId iid);

    // Resource access
    [[nodiscard]] virtual std::vector<ResourceId> list_resources(InstanceId iid) const = 0;

    // Get resource type for TLV decoding
    [[nodiscard]] virtual std::optional<ResourceType> get_resource_type(
        InstanceId iid, ResourceId rid) const;

    [[nodiscard]] virtual Result<ResourceValue> read_resource(
        InstanceId iid,
        ResourceId rid,
        std::optional<ResourceInstanceId> riid = std::nullopt
    ) const = 0;

    [[nodiscard]] virtual Result<void> write_resource(
        InstanceId iid,
        ResourceId rid,
        std::optional<ResourceInstanceId> riid,
        const ResourceValue& value
    ) = 0;

    [[nodiscard]] virtual Result<void> execute_resource(
        InstanceId iid,
        ResourceId rid,
        std::string_view arguments
    );

    // Transactions (for atomic operations)
    virtual Result<void> begin_transaction();
    virtual Result<void> commit_transaction();
    virtual Result<void> rollback_transaction();

protected:
    // Helper for derived classes to register resources
    void register_resource(Resource resource);
    [[nodiscard]] const Resource* find_resource(ResourceId rid) const;

private:
    std::unordered_map<uint16_t, Resource> resources_;
};

// CRTP helper for single-instance objects
template<typename Derived, uint16_t OID>
class SingleInstanceObject : public Object {
public:
    [[nodiscard]] ObjectId id() const noexcept final {
        return ObjectId{OID};
    }

    [[nodiscard]] std::vector<InstanceId> list_instances() const final {
        return {InstanceId{0}};
    }

    [[nodiscard]] bool has_instance(InstanceId iid) const final {
        return iid.value == 0;
    }

    [[nodiscard]] Result<InstanceId> create_instance(std::optional<InstanceId>) final {
        return Err<InstanceId>(ErrorCode::MethodNotAllowed, "Single instance object");
    }

    Result<void> delete_instance(InstanceId) final {
        return Err<void>(ErrorCode::MethodNotAllowed, "Single instance object");
    }
};

} // namespace lwm2m
