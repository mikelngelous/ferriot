#pragma once

// Ferriot - SenML JSON codec (RFC 8428, content format 110).

#include "codec.hpp"

namespace lwm2m::codec {

class SenmlJsonCodec : public DataCodec {
public:
    [[nodiscard]] transport::ContentFormat format() const noexcept override {
        return transport::ContentFormat::SenmlJson;
    }

    [[nodiscard]] Result<std::vector<uint8_t>> encode_read(
        const ObjectPath& base, const std::vector<ReadEntry>& entries) override;

    [[nodiscard]] Result<std::vector<ResourceRecord>> decode_write(
        const std::vector<uint8_t>& data, const TypeResolver& type_of) override;
};

}  // namespace lwm2m::codec
