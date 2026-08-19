// Ferriot - Codec selection by content format

#include "lwm2m/codec/codec.hpp"
#include "lwm2m/codec/tlv_codec.hpp"

namespace lwm2m::codec {

std::unique_ptr<DataCodec> select_codec(transport::ContentFormat format) {
    switch (format) {
        case transport::ContentFormat::TlvLwm2m:
        default:
            return std::make_unique<TlvCodec>();
    }
}

}  // namespace lwm2m::codec
