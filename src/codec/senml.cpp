// Ferriot - SenML (RFC 8428) mapping shared by the JSON and CBOR codecs.

#include "lwm2m/codec/senml.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>

namespace lwm2m::codec {

namespace {

std::string dir_of(const ObjectPath& base) {
    std::string s = "/" + std::to_string(base.object_id.value) + "/";
    if (!base.is_object()) {
        s += std::to_string(base.instance_id->value) + "/";
    }
    return s;
}

ResourceValue coerce(const ResourceValue& v, ResourceType type) {
    auto as_double = [&](double& out) -> bool {
        if (auto* i = std::get_if<int64_t>(&v)) { out = static_cast<double>(*i); return true; }
        if (auto* u = std::get_if<uint64_t>(&v)) { out = static_cast<double>(*u); return true; }
        if (auto* d = std::get_if<double>(&v)) { out = *d; return true; }
        return false;
    };

    switch (type) {
        case ResourceType::Integer: {
            double d;
            if (as_double(d)) return ResourceValue{static_cast<int64_t>(std::llround(d))};
            break;
        }
        case ResourceType::UnsignedInteger: {
            double d;
            if (as_double(d)) return ResourceValue{static_cast<uint64_t>(std::llround(d))};
            break;
        }
        case ResourceType::Float: {
            double d;
            if (as_double(d)) return ResourceValue{d};
            break;
        }
        case ResourceType::Time: {
            double d;
            if (as_double(d)) {
                return ResourceValue{std::chrono::system_clock::time_point{
                    std::chrono::seconds{static_cast<int64_t>(std::llround(d))}}};
            }
            break;
        }
        default:
            break;
    }
    return v;
}

constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

}  // namespace

SenmlPayload build_senml_payload(const ObjectPath& base, const std::vector<ReadEntry>& entries) {
    SenmlPayload out;
    out.base_name = dir_of(base);

    const bool object_level = base.is_object();
    out.items.reserve(entries.size());
    for (const auto& e : entries) {
        std::string name = object_level
            ? std::to_string(e.iid.value) + "/" + std::to_string(e.rid.value)
            : std::to_string(e.rid.value);
        out.items.push_back({std::move(name), e.value});
    }
    return out;
}

Result<std::vector<ResourceRecord>> resolve_senml(
    const std::vector<SenmlInRecord>& records, const TypeResolver& type_of) {
    std::vector<ResourceRecord> out;
    out.reserve(records.size());

    std::string running_base;
    for (const auto& rec : records) {
        if (rec.bn) {
            running_base = *rec.bn;
        }
        const std::string full = running_base + rec.n;

        auto path = ObjectPath::parse(full);
        if (!path || !path->resource_id) {
            return Err<std::vector<ResourceRecord>>(
                ErrorCode::DecodingError, "SenML: record name is not a resource path: " + full);
        }

        ResourceValue value = rec.value;
        if (auto type = type_of(*path->resource_id)) {
            value = coerce(value, *type);
        }
        out.push_back(ResourceRecord{*path->resource_id, path->resource_instance_id, std::move(value)});
    }
    return Ok(std::move(out));
}

std::string base64url_encode(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const uint32_t n = (uint32_t{data[i]} << 16) | (uint32_t{data[i + 1]} << 8) | data[i + 2];
        out.push_back(kB64[(n >> 18) & 0x3F]);
        out.push_back(kB64[(n >> 12) & 0x3F]);
        out.push_back(kB64[(n >> 6) & 0x3F]);
        out.push_back(kB64[n & 0x3F]);
    }
    const size_t rem = data.size() - i;
    if (rem == 1) {
        const uint32_t n = uint32_t{data[i]} << 16;
        out.push_back(kB64[(n >> 18) & 0x3F]);
        out.push_back(kB64[(n >> 12) & 0x3F]);
    } else if (rem == 2) {
        const uint32_t n = (uint32_t{data[i]} << 16) | (uint32_t{data[i + 1]} << 8);
        out.push_back(kB64[(n >> 18) & 0x3F]);
        out.push_back(kB64[(n >> 12) & 0x3F]);
        out.push_back(kB64[(n >> 6) & 0x3F]);
    }
    return out;
}

Result<ObjectPath> parse_objlink(const std::string& s) {
    const auto colon = s.find(':');
    if (colon == std::string::npos) {
        return Err<ObjectPath>(ErrorCode::DecodingError, "SenML: bad objlink '" + s + "'");
    }
    ObjectPath p;
    p.object_id = ObjectId{static_cast<uint16_t>(std::strtoul(s.c_str(), nullptr, 10))};
    p.instance_id = InstanceId{static_cast<uint16_t>(std::strtoul(s.c_str() + colon + 1, nullptr, 10))};
    return Ok(p);
}

Result<std::vector<uint8_t>> base64url_decode(const std::string& s) {
    std::array<int8_t, 256> rev;
    rev.fill(-1);
    for (int8_t c = 0; c < 64; ++c) {
        rev[static_cast<uint8_t>(kB64[c])] = c;
    }

    std::vector<uint8_t> out;
    out.reserve(s.size() * 3 / 4);
    uint32_t buf = 0;
    int bits = 0;
    for (char ch : s) {
        if (ch == '=') break;
        const int8_t v = rev[static_cast<uint8_t>(ch)];
        if (v < 0) {
            return Err<std::vector<uint8_t>>(ErrorCode::DecodingError, "base64url: invalid character");
        }
        buf = (buf << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return Ok(std::move(out));
}

}  // namespace lwm2m::codec
