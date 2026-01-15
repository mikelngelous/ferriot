// Ferriot - Types Implementation

#include "lwm2m/types.hpp"

#include <charconv>
#include <sstream>

namespace lwm2m {

std::string ObjectPath::to_string() const {
    std::ostringstream oss;
    oss << "/" << object_id.value;

    if (instance_id.has_value()) {
        oss << "/" << instance_id->value;

        if (resource_id.has_value()) {
            oss << "/" << resource_id->value;

            if (resource_instance_id.has_value()) {
                oss << "/" << resource_instance_id->value;
            }
        }
    }

    return oss.str();
}

std::optional<ObjectPath> ObjectPath::parse(std::string_view path) {
    if (path.empty() || path[0] != '/') {
        return std::nullopt;
    }

    // Reject trailing slash (implies empty segment)
    if (path.size() > 1 && path.back() == '/') {
        return std::nullopt;
    }

    ObjectPath result{};
    size_t pos = 1;
    int component = 0;

    while (pos < path.size() && component < 4) {
        size_t end = path.find('/', pos);
        if (end == std::string_view::npos) {
            end = path.size();
        }

        std::string_view segment = path.substr(pos, end - pos);
        if (segment.empty()) {
            return std::nullopt;
        }

        uint16_t value = 0;
        auto [ptr, ec] = std::from_chars(segment.data(), segment.data() + segment.size(), value);
        if (ec != std::errc{} || ptr != segment.data() + segment.size()) {
            return std::nullopt;
        }

        switch (component) {
            case 0:
                result.object_id = ObjectId{value};
                break;
            case 1:
                result.instance_id = InstanceId{value};
                break;
            case 2:
                result.resource_id = ResourceId{value};
                break;
            case 3:
                result.resource_instance_id = ResourceInstanceId{value};
                break;
        }

        ++component;
        pos = end + 1;
    }

    if (component == 0) {
        return std::nullopt;
    }

    return result;
}

} // namespace lwm2m
