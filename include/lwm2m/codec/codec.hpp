#pragma once

// Ferriot - Data codec interface (TLV, SenML JSON, SenML CBOR).

#include "../types.hpp"
#include "../resource.hpp"
#include "../result.hpp"
#include "../transport/coap_client.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace lwm2m::codec {

struct ReadEntry {
    InstanceId iid;
    ResourceId rid;
    ResourceValue value;
};

struct ResourceRecord {
    ResourceId rid;
    std::optional<ResourceInstanceId> riid;
    ResourceValue value;
};

// TLV needs the expected type (the wire carries none); SenML records are self-describing.
using TypeResolver = std::function<std::optional<ResourceType>(ResourceId)>;

class DataCodec {
public:
    virtual ~DataCodec() = default;

    [[nodiscard]] virtual transport::ContentFormat format() const noexcept = 0;

    [[nodiscard]] virtual Result<std::vector<uint8_t>> encode_read(
        const ObjectPath& base, const std::vector<ReadEntry>& entries) = 0;

    [[nodiscard]] virtual Result<std::vector<ResourceRecord>> decode_write(
        const std::vector<uint8_t>& data, const TypeResolver& type_of) = 0;
};

[[nodiscard]] std::unique_ptr<DataCodec> select_codec(transport::ContentFormat format);

}  // namespace lwm2m::codec
