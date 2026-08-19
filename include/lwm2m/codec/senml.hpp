#pragma once

// Ferriot - SenML (RFC 8428) mapping shared by the JSON and CBOR codecs.

#include "codec.hpp"

#include <optional>
#include <string>
#include <vector>

namespace lwm2m::codec {

struct SenmlPayload {
    struct Item {
        std::string name;
        ResourceValue value;
    };
    std::string base_name;
    std::vector<Item> items;
};

[[nodiscard]] SenmlPayload build_senml_payload(
    const ObjectPath& base, const std::vector<ReadEntry>& entries);

// `bn` is sticky across the array per RFC 8428; resolve_senml concatenates it with `n`.
struct SenmlInRecord {
    std::optional<std::string> bn;
    std::string n;
    ResourceValue value;
};

[[nodiscard]] Result<std::vector<ResourceRecord>> resolve_senml(
    const std::vector<SenmlInRecord>& records, const TypeResolver& type_of);

[[nodiscard]] std::string base64url_encode(const std::vector<uint8_t>& data);
[[nodiscard]] Result<std::vector<uint8_t>> base64url_decode(const std::string& s);

[[nodiscard]] Result<ObjectPath> parse_objlink(const std::string& s);

}  // namespace lwm2m::codec
