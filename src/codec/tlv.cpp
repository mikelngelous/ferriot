// Ferriot - TLV Codec Implementation

#include "lwm2m/codec/tlv.hpp"

#include <cstring>

namespace lwm2m::codec {

// TLV Encoder Implementation

Result<std::vector<uint8_t>> TlvEncoder::encode_resource(
    ResourceId rid,
    const ResourceValue& value
) {
    std::vector<uint8_t> serialized = std::visit([this](const auto& v) -> std::vector<uint8_t> {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return {};
        } else if constexpr (std::is_same_v<T, bool>) {
            return serialize_bool(v);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return serialize_int(v);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return serialize_uint(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return serialize_float(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return serialize_string(v);
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return v;  // Opaque data as-is
        } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
            return serialize_time(v);
        } else if constexpr (std::is_same_v<T, ObjectPath>) {
            return serialize_objlink(v);
        } else {
            return {};
        }
    }, value);

    return Ok(encode_record(TlvType::Resource, rid.value, serialized));
}

Result<std::vector<uint8_t>> TlvEncoder::encode_multi_resource(
    ResourceId rid,
    const std::vector<std::pair<ResourceInstanceId, ResourceValue>>& instances
) {
    std::vector<uint8_t> inner;

    for (const auto& [riid, value] : instances) {
        auto res = encode_resource(ResourceId{riid.value}, value);
        if (!res) {
            return Err<std::vector<uint8_t>>(res.error());
        }
        // Change type to ResourceInstance
        auto& data = res.value();
        if (!data.empty()) {
            data[0] = static_cast<uint8_t>((data[0] & 0x3F) | (static_cast<uint8_t>(TlvType::ResourceInstance) << 6));
        }
        inner.insert(inner.end(), data.begin(), data.end());
    }

    return Ok(encode_record(TlvType::MultiResource, rid.value, inner));
}

Result<std::vector<uint8_t>> TlvEncoder::encode_instance(
    InstanceId iid,
    const std::vector<std::pair<ResourceId, ResourceValue>>& resources
) {
    std::vector<uint8_t> inner;

    for (const auto& [rid, value] : resources) {
        auto res = encode_resource(rid, value);
        if (!res) {
            return Err<std::vector<uint8_t>>(res.error());
        }
        inner.insert(inner.end(), res.value().begin(), res.value().end());
    }

    return Ok(encode_record(TlvType::ObjectInstance, iid.value, inner));
}

std::vector<uint8_t> TlvEncoder::encode_record(
    TlvType type,
    uint16_t id,
    const std::vector<uint8_t>& value
) {
    std::vector<uint8_t> result;

    // Type byte: bits 7-6 = type, bit 5 = id length, bits 4-3 = length type
    uint8_t type_byte = static_cast<uint8_t>(static_cast<uint8_t>(type) << 6);

    // ID length (bit 5): 0 = 8-bit, 1 = 16-bit
    bool id_16bit = id > 255;
    if (id_16bit) {
        type_byte |= 0x20;
    }

    // Determine length encoding and set bits 4-3 in type byte
    size_t len = value.size();
    if (len < 8) {
        // Inline length (bits 2-0)
        type_byte |= static_cast<uint8_t>(len & 0x07);
    } else if (len < 256) {
        // 8-bit length field
        type_byte |= 0x08;
    } else if (len < 65536) {
        // 16-bit length field
        type_byte |= 0x10;
    } else {
        // 24-bit length field
        type_byte |= 0x18;
    }

    // Write type byte
    result.push_back(type_byte);

    // Write ID (MUST come before length per OMA LWM2M TLV spec)
    if (id_16bit) {
        result.push_back(static_cast<uint8_t>((id >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(id & 0xFF));
    } else {
        result.push_back(static_cast<uint8_t>(id));
    }

    // Write length field (if not inline)
    if (len >= 8 && len < 256) {
        result.push_back(static_cast<uint8_t>(len));
    } else if (len >= 256 && len < 65536) {
        result.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(len & 0xFF));
    } else if (len >= 65536) {
        result.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(len & 0xFF));
    }

    // Write value
    result.insert(result.end(), value.begin(), value.end());

    return result;
}

std::vector<uint8_t> TlvEncoder::serialize_bool(bool value) {
    return {value ? uint8_t{1} : uint8_t{0}};
}

std::vector<uint8_t> TlvEncoder::serialize_int(int64_t value) {
    // Variable-length signed integer (big-endian)
    if (value >= -128 && value <= 127) {
        return {static_cast<uint8_t>(value)};
    } else if (value >= -32768 && value <= 32767) {
        return {
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
    } else if (value >= -8388608 && value <= 8388607) {
        return {
            static_cast<uint8_t>((value >> 24) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
    } else {
        return {
            static_cast<uint8_t>((value >> 56) & 0xFF),
            static_cast<uint8_t>((value >> 48) & 0xFF),
            static_cast<uint8_t>((value >> 40) & 0xFF),
            static_cast<uint8_t>((value >> 32) & 0xFF),
            static_cast<uint8_t>((value >> 24) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
    }
}

std::vector<uint8_t> TlvEncoder::serialize_uint(uint64_t value) {
    // Variable-length unsigned integer (big-endian)
    if (value <= 255) {
        return {static_cast<uint8_t>(value)};
    } else if (value <= 65535) {
        return {
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
    } else if (value <= 16777215) {
        return {
            static_cast<uint8_t>((value >> 24) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
    } else {
        return {
            static_cast<uint8_t>((value >> 56) & 0xFF),
            static_cast<uint8_t>((value >> 48) & 0xFF),
            static_cast<uint8_t>((value >> 40) & 0xFF),
            static_cast<uint8_t>((value >> 32) & 0xFF),
            static_cast<uint8_t>((value >> 24) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
    }
}

std::vector<uint8_t> TlvEncoder::serialize_float(double value) {
    // IEEE 754 double precision (8 bytes, big-endian)
    std::vector<uint8_t> result(8);
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));

    for (int i = 7; i >= 0; --i) {
        result[static_cast<size_t>(7 - i)] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFF);
    }

    return result;
}

std::vector<uint8_t> TlvEncoder::serialize_string(std::string_view value) {
    return std::vector<uint8_t>(value.begin(), value.end());
}

std::vector<uint8_t> TlvEncoder::serialize_time(std::chrono::system_clock::time_point value) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        value.time_since_epoch()
    ).count();
    return serialize_int(seconds);
}

std::vector<uint8_t> TlvEncoder::serialize_objlink(const ObjectPath& value) {
    // Object Link: 4 bytes (2 for object ID, 2 for instance ID)
    uint16_t oid = value.object_id.value;
    uint16_t iid = value.instance_id.value_or(InstanceId{0}).value;

    return {
        static_cast<uint8_t>((oid >> 8) & 0xFF),
        static_cast<uint8_t>(oid & 0xFF),
        static_cast<uint8_t>((iid >> 8) & 0xFF),
        static_cast<uint8_t>(iid & 0xFF)
    };
}

// TLV Decoder Implementation

// LWM2M nesting is Object->Instance->MultiResource->ResourceInstance; 8 is ample
static constexpr unsigned kMaxTlvDepth = 8;

Result<std::vector<TlvRecord>> TlvDecoder::decode(const std::vector<uint8_t>& data) {
    return decode_impl(data, 0);
}

Result<std::vector<TlvRecord>> TlvDecoder::decode_impl(const std::vector<uint8_t>& data, unsigned depth) {
    if (depth > kMaxTlvDepth) {
        return Err<std::vector<TlvRecord>>(ErrorCode::DecodingError, "TLV nesting too deep");
    }

    std::vector<TlvRecord> records;
    size_t pos = 0;

    while (pos < data.size()) {
        auto result = parse_record(data.data() + pos, data.size() - pos, depth);
        if (!result) {
            return Err<std::vector<TlvRecord>>(result.error());
        }

        auto& [record, consumed] = result.value();
        records.push_back(std::move(record));
        pos += consumed;
    }

    return Ok(std::move(records));
}

Result<std::pair<TlvRecord, size_t>> TlvDecoder::parse_record(const uint8_t* data, size_t len, unsigned depth) {
    if (len == 0) {
        return Err<std::pair<TlvRecord, size_t>>(ErrorCode::DecodingError, "Empty TLV data");
    }

    uint8_t type_byte = data[0];
    size_t pos = 1;

    // Extract type (bits 7-6)
    auto type = static_cast<TlvType>((type_byte >> 6) & 0x03);

    // ID length (bit 5): 0 = 8-bit, 1 = 16-bit
    bool id_16bit = (type_byte & 0x20) != 0;

    // Length type (bits 4-3)
    uint8_t length_type = (type_byte >> 3) & 0x03;

    // Parse ID first (per OMA LWM2M TLV spec: Type, ID, Length, Value)
    uint16_t id = 0;
    if (id_16bit) {
        if (pos + 1 >= len) {
            return Err<std::pair<TlvRecord, size_t>>(ErrorCode::DecodingError, "Truncated TLV");
        }
        id = static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
        pos += 2;
    } else {
        if (pos >= len) {
            return Err<std::pair<TlvRecord, size_t>>(ErrorCode::DecodingError, "Truncated TLV");
        }
        id = data[pos++];
    }

    // Parse length field (comes after ID)
    size_t value_length = 0;
    if (length_type == 0) {
        // Inline length (bits 2-0)
        value_length = type_byte & 0x07;
    } else if (length_type == 1) {
        // 8-bit length
        if (pos >= len) {
            return Err<std::pair<TlvRecord, size_t>>(ErrorCode::DecodingError, "Truncated TLV");
        }
        value_length = data[pos++];
    } else if (length_type == 2) {
        // 16-bit length
        if (pos + 1 >= len) {
            return Err<std::pair<TlvRecord, size_t>>(ErrorCode::DecodingError, "Truncated TLV");
        }
        value_length = (static_cast<size_t>(data[pos]) << 8) | data[pos + 1];
        pos += 2;
    } else {
        // 24-bit length
        if (pos + 2 >= len) {
            return Err<std::pair<TlvRecord, size_t>>(ErrorCode::DecodingError, "Truncated TLV");
        }
        value_length = (static_cast<size_t>(data[pos]) << 16) |
                       (static_cast<size_t>(data[pos + 1]) << 8) |
                       data[pos + 2];
        pos += 3;
    }

    // Extract value
    if (pos + value_length > len) {
        return Err<std::pair<TlvRecord, size_t>>(ErrorCode::DecodingError, "Truncated TLV value");
    }

    TlvRecord record;
    record.type = type;
    record.id = id;
    record.value = std::vector<uint8_t>(data + pos, data + pos + value_length);

    // Parse nested records for ObjectInstance and MultiResource types
    if (type == TlvType::ObjectInstance || type == TlvType::MultiResource) {
        auto nested = decode_impl(record.value, depth + 1);
        if (nested) {
            record.children = std::move(nested.value());
        }
    }

    return Ok(std::make_pair(std::move(record), pos + value_length));
}

Result<ResourceValue> TlvDecoder::decode_resource(
    const std::vector<uint8_t>& data,
    ResourceType expected_type
) {
    switch (expected_type) {
        case ResourceType::Boolean:
            return deserialize_bool(data.data(), data.size()).map([](bool v) -> ResourceValue { return v; });
        case ResourceType::Integer:
            return deserialize_int(data.data(), data.size()).map([](int64_t v) -> ResourceValue { return v; });
        case ResourceType::UnsignedInteger:
            return deserialize_uint(data.data(), data.size()).map([](uint64_t v) -> ResourceValue { return v; });
        case ResourceType::Float:
            return deserialize_float(data.data(), data.size()).map([](double v) -> ResourceValue { return v; });
        case ResourceType::String:
            return deserialize_string(data.data(), data.size()).map([](std::string v) -> ResourceValue { return v; });
        case ResourceType::Opaque:
            return Ok<ResourceValue>(data);
        case ResourceType::Time:
            return deserialize_time(data.data(), data.size()).map([](auto v) -> ResourceValue { return v; });
        case ResourceType::ObjectLink:
            return deserialize_objlink(data.data(), data.size()).map([](ObjectPath v) -> ResourceValue { return v; });
        case ResourceType::None:
        default:
            return Ok<ResourceValue>(std::monostate{});
    }
}

Result<bool> TlvDecoder::deserialize_bool(const uint8_t* data, size_t len) {
    if (len == 0) {
        return Err<bool>(ErrorCode::DecodingError, "Empty boolean value");
    }
    return Ok(data[0] != 0);
}

Result<int64_t> TlvDecoder::deserialize_int(const uint8_t* data, size_t len) {
    if (len == 0 || len > 8) {
        return Err<int64_t>(ErrorCode::DecodingError, "Invalid integer length");
    }

    int64_t value = 0;
    bool negative = (data[0] & 0x80) != 0;

    for (size_t i = 0; i < len; ++i) {
        value = (value << 8) | data[i];
    }

    // Sign extend if necessary
    if (negative && len < 8) {
        int64_t mask = ~((int64_t{1} << (len * 8)) - 1);
        value |= mask;
    }

    return Ok(value);
}

Result<uint64_t> TlvDecoder::deserialize_uint(const uint8_t* data, size_t len) {
    if (len == 0 || len > 8) {
        return Err<uint64_t>(ErrorCode::DecodingError, "Invalid integer length");
    }

    uint64_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        value = (value << 8) | data[i];
    }

    return Ok(value);
}

Result<double> TlvDecoder::deserialize_float(const uint8_t* data, size_t len) {
    if (len == 4) {
        // Single precision
        uint32_t bits = 0;
        for (size_t i = 0; i < len; ++i) {
            bits = (bits << 8) | data[i];
        }
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return Ok(static_cast<double>(value));
    } else if (len == 8) {
        // Double precision
        uint64_t bits = 0;
        for (size_t i = 0; i < len; ++i) {
            bits = (bits << 8) | data[i];
        }
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        return Ok(value);
    }

    return Err<double>(ErrorCode::DecodingError, "Invalid float length");
}

Result<std::string> TlvDecoder::deserialize_string(const uint8_t* data, size_t len) {
    return Ok(std::string(reinterpret_cast<const char*>(data), len));
}

Result<std::chrono::system_clock::time_point> TlvDecoder::deserialize_time(
    const uint8_t* data, size_t len
) {
    auto seconds = deserialize_int(data, len);
    if (!seconds) {
        return Err<std::chrono::system_clock::time_point>(seconds.error());
    }

    return Ok(std::chrono::system_clock::time_point{
        std::chrono::seconds{seconds.value()}
    });
}

Result<ObjectPath> TlvDecoder::deserialize_objlink(const uint8_t* data, size_t len) {
    if (len != 4) {
        return Err<ObjectPath>(ErrorCode::DecodingError, "Invalid object link length");
    }

    uint16_t oid = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
    uint16_t iid = static_cast<uint16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);

    return Ok(ObjectPath::instance(ObjectId{oid}, InstanceId{iid}));
}

} // namespace lwm2m::codec
