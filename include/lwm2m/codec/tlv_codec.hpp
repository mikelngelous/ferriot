#pragma once

// Ferriot - TLV codec (OMA LWM2M TLV, content format 11542).

#include "codec.hpp"

namespace lwm2m::codec {

class TlvCodec : public DataCodec {
public:
    [[nodiscard]] transport::ContentFormat format() const noexcept override {
        return transport::ContentFormat::TlvLwm2m;
    }

    [[nodiscard]] Result<std::vector<uint8_t>> encode_read(
        const ObjectPath& base, const std::vector<ReadEntry>& entries) override;

    [[nodiscard]] Result<std::vector<ResourceRecord>> decode_write(
        const std::vector<uint8_t>& data, const TypeResolver& type_of) override;
};

}  // namespace lwm2m::codec
