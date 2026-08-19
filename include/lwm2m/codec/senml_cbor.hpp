#pragma once

// Ferriot - SenML CBOR codec (RFC 8428 + RFC 8949, content format 112).

#include "codec.hpp"

namespace lwm2m::codec {

class SenmlCborCodec : public DataCodec {
public:
    [[nodiscard]] transport::ContentFormat format() const noexcept override {
        return transport::ContentFormat::SenmlCbor;
    }

    [[nodiscard]] Result<std::vector<uint8_t>> encode_read(
        const ObjectPath& base, const std::vector<ReadEntry>& entries) override;

    [[nodiscard]] Result<std::vector<ResourceRecord>> decode_write(
        const std::vector<uint8_t>& data, const TypeResolver& type_of) override;
};

}  // namespace lwm2m::codec
