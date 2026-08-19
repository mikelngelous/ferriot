// Ferriot - TLV codec adapter

#include "lwm2m/codec/tlv_codec.hpp"
#include "lwm2m/codec/tlv.hpp"

#include <map>
#include <utility>

namespace lwm2m::codec {

Result<std::vector<uint8_t>> TlvCodec::encode_read(
    const ObjectPath& base, const std::vector<ReadEntry>& entries) {
    TlvEncoder encoder;

    if (base.is_resource()) {
        if (entries.empty()) {
            return Ok(std::vector<uint8_t>{});
        }
        return encoder.encode_resource(entries.front().rid, entries.front().value);
    }

    std::map<uint16_t, std::vector<std::pair<ResourceId, ResourceValue>>> by_instance;
    std::vector<uint16_t> order;
    for (const auto& e : entries) {
        if (by_instance.find(e.iid.value) == by_instance.end()) {
            order.push_back(e.iid.value);
        }
        by_instance[e.iid.value].emplace_back(e.rid, e.value);
    }

    std::vector<uint8_t> out;
    for (uint16_t iid_val : order) {
        auto encoded = encoder.encode_instance(InstanceId{iid_val}, by_instance[iid_val]);
        if (!encoded) {
            return Err<std::vector<uint8_t>>(encoded.error());
        }
        out.insert(out.end(), encoded.value().begin(), encoded.value().end());
    }
    return Ok(std::move(out));
}

Result<std::vector<ResourceRecord>> TlvCodec::decode_write(
    const std::vector<uint8_t>& data, const TypeResolver& type_of) {
    TlvDecoder decoder;
    auto records = decoder.decode(data);
    if (!records) {
        return Err<std::vector<ResourceRecord>>(records.error());
    }

    std::vector<ResourceRecord> out;
    for (const auto& rec : records.value()) {
        if (rec.type != TlvType::Resource) {
            continue;
        }
        ResourceId rid{rec.id};
        ResourceType expected = type_of(rid).value_or(ResourceType::String);
        auto value = decoder.decode_resource(rec.value, expected);
        if (!value) {
            return Err<std::vector<ResourceRecord>>(value.error());
        }
        out.push_back(ResourceRecord{rid, std::nullopt, std::move(value.value())});
    }
    return Ok(std::move(out));
}

}  // namespace lwm2m::codec
