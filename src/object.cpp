// Ferriot - Object Implementation

#include "lwm2m/object.hpp"

namespace lwm2m {

Result<InstanceId> Object::create_instance(std::optional<InstanceId>) {
    return Err<InstanceId>(ErrorCode::MethodNotAllowed, "Object does not support instance creation");
}

Result<void> Object::delete_instance(InstanceId) {
    return Err<void>(ErrorCode::MethodNotAllowed, "Object does not support instance deletion");
}

Result<void> Object::execute_resource(InstanceId, ResourceId, std::string_view) {
    return Err<void>(ErrorCode::MethodNotAllowed, "Resource is not executable");
}

std::optional<ResourceType> Object::get_resource_type(InstanceId /*iid*/, ResourceId rid) const {
    const Resource* res = find_resource(rid);
    if (res) {
        return res->type();
    }
    return std::nullopt;
}

Result<void> Object::begin_transaction() {
    return Ok();
}

Result<void> Object::commit_transaction() {
    return Ok();
}

Result<void> Object::rollback_transaction() {
    return Ok();
}

void Object::register_resource(Resource resource) {
    resources_.emplace(resource.id().value, std::move(resource));
}

const Resource* Object::find_resource(ResourceId rid) const {
    auto it = resources_.find(rid.value);
    if (it != resources_.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace lwm2m
