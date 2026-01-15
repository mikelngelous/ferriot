#pragma once

// Ferriot - TLV Codec
// OMA LWM2M TLV (Type-Length-Value) serialization

#include "../types.hpp"
#include "../result.hpp"
#include "../resource.hpp"

#include <cstdint>
#include <vector>

namespace lwm2m::codec {

// TLV type identifiers (OMA LWM2M spec section 6.4.3)
enum class TlvType : uint8_t {
    ObjectInstance   = 0b00,
    ResourceInstance = 0b01,
    MultiResource    = 0b10,
    Resource         = 0b11,
};

// Decoded TLV record
struct TlvRecord {
    TlvType type;
    uint16_t id;
    std::vector<uint8_t> value;
    std::vector<TlvRecord> children;  // For nested structures
};

// TLV Encoder - serialize LWM2M data to TLV format
class TlvEncoder {
public:
    TlvEncoder() = default;

    // Encode a single resource value
    [[nodiscard]] Result<std::vector<uint8_t>> encode_resource(
        ResourceId rid,
        const ResourceValue& value
    );

    // Encode a multi-instance resource
    [[nodiscard]] Result<std::vector<uint8_t>> encode_multi_resource(
        ResourceId rid,
        const std::vector<std::pair<ResourceInstanceId, ResourceValue>>& instances
    );

    // Encode an object instance
    [[nodiscard]] Result<std::vector<uint8_t>> encode_instance(
        InstanceId iid,
        const std::vector<std::pair<ResourceId, ResourceValue>>& resources
    );

    // Low-level: encode raw TLV record
    [[nodiscard]] std::vector<uint8_t> encode_record(
        TlvType type,
        uint16_t id,
        const std::vector<uint8_t>& value
    );

private:
    // Value serializers
    [[nodiscard]] std::vector<uint8_t> serialize_bool(bool value);
    [[nodiscard]] std::vector<uint8_t> serialize_int(int64_t value);
    [[nodiscard]] std::vector<uint8_t> serialize_uint(uint64_t value);
    [[nodiscard]] std::vector<uint8_t> serialize_float(double value);
    [[nodiscard]] std::vector<uint8_t> serialize_string(std::string_view value);
    [[nodiscard]] std::vector<uint8_t> serialize_time(std::chrono::system_clock::time_point value);
    [[nodiscard]] std::vector<uint8_t> serialize_objlink(const ObjectPath& value);
};

// TLV Decoder - parse TLV format to LWM2M data
class TlvDecoder {
public:
    TlvDecoder() = default;

    // Parse TLV buffer into records
    [[nodiscard]] Result<std::vector<TlvRecord>> decode(const std::vector<uint8_t>& data);

    // Parse a single resource value from TLV
    [[nodiscard]] Result<ResourceValue> decode_resource(
        const std::vector<uint8_t>& data,
        ResourceType expected_type
    );

private:
    // Parse single TLV record at position, returns bytes consumed
    [[nodiscard]] Result<std::pair<TlvRecord, size_t>> parse_record(
        const uint8_t* data, size_t len
    );

    // Value deserializers
    [[nodiscard]] Result<bool> deserialize_bool(const uint8_t* data, size_t len);
    [[nodiscard]] Result<int64_t> deserialize_int(const uint8_t* data, size_t len);
    [[nodiscard]] Result<uint64_t> deserialize_uint(const uint8_t* data, size_t len);
    [[nodiscard]] Result<double> deserialize_float(const uint8_t* data, size_t len);
    [[nodiscard]] Result<std::string> deserialize_string(const uint8_t* data, size_t len);
    [[nodiscard]] Result<std::chrono::system_clock::time_point> deserialize_time(
        const uint8_t* data, size_t len
    );
    [[nodiscard]] Result<ObjectPath> deserialize_objlink(const uint8_t* data, size_t len);
};

} // namespace lwm2m::codec
