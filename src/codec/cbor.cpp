// Ferriot - Minimal CBOR (RFC 8949) reader/writer.

#include "lwm2m/codec/cbor.hpp"

#include <cmath>
#include <cstring>
#include <limits>

namespace lwm2m::codec {

namespace {
constexpr uint8_t kSimpleFalse = 20;
constexpr uint8_t kSimpleTrue = 21;
constexpr uint8_t kAiFloat64 = 27;
constexpr uint8_t kAiFloat32 = 26;
constexpr uint8_t kAiFloat16 = 25;
}  // namespace

void CborWriter::write_head(CborMajor major, uint64_t arg) {
    const uint8_t mt = static_cast<uint8_t>(static_cast<uint8_t>(major) << 5);
    if (arg < 24) {
        buf_.push_back(static_cast<uint8_t>(mt | arg));
    } else if (arg <= 0xFF) {
        buf_.push_back(mt | 24);
        buf_.push_back(static_cast<uint8_t>(arg));
    } else if (arg <= 0xFFFF) {
        buf_.push_back(mt | 25);
        buf_.push_back(static_cast<uint8_t>(arg >> 8));
        buf_.push_back(static_cast<uint8_t>(arg));
    } else if (arg <= 0xFFFFFFFF) {
        buf_.push_back(mt | 26);
        for (int shift = 24; shift >= 0; shift -= 8) {
            buf_.push_back(static_cast<uint8_t>(arg >> shift));
        }
    } else {
        buf_.push_back(mt | 27);
        for (int shift = 56; shift >= 0; shift -= 8) {
            buf_.push_back(static_cast<uint8_t>(arg >> shift));
        }
    }
}

void CborWriter::write_int(int64_t v) {
    if (v >= 0) {
        write_head(CborMajor::Unsigned, static_cast<uint64_t>(v));
    } else {
        write_head(CborMajor::Negative, static_cast<uint64_t>(-(v + 1)));
    }
}

void CborWriter::write_bool(bool v) {
    buf_.push_back((static_cast<uint8_t>(CborMajor::Simple) << 5) |
                   (v ? kSimpleTrue : kSimpleFalse));
}

void CborWriter::write_double(double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    buf_.push_back((static_cast<uint8_t>(CborMajor::Simple) << 5) | kAiFloat64);
    for (int shift = 56; shift >= 0; shift -= 8) {
        buf_.push_back(static_cast<uint8_t>(bits >> shift));
    }
}

void CborWriter::write_text(std::string_view s) {
    write_head(CborMajor::Text, s.size());
    buf_.insert(buf_.end(), s.begin(), s.end());
}

void CborWriter::write_bytes(const std::vector<uint8_t>& b) {
    write_head(CborMajor::Bytes, b.size());
    buf_.insert(buf_.end(), b.begin(), b.end());
}

Result<uint64_t> CborReader::read_bytes_be(size_t n) {
    if (pos_ + n > data_.size()) {
        return Err<uint64_t>(ErrorCode::DecodingError, "CBOR: truncated argument");
    }
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i) {
        v = (v << 8) | data_[pos_++];
    }
    return Ok(v);
}

Result<std::pair<CborMajor, uint64_t>> CborReader::read_head() {
    if (pos_ >= data_.size()) {
        return Err<std::pair<CborMajor, uint64_t>>(ErrorCode::DecodingError, "CBOR: unexpected end");
    }
    const uint8_t initial = data_[pos_++];
    const CborMajor major = static_cast<CborMajor>(initial >> 5);
    const uint8_t ai = initial & 0x1F;

    uint64_t arg;
    if (ai < 24) {
        arg = ai;
    } else if (ai == 24) {
        auto b = read_bytes_be(1);
        if (!b) return Err<std::pair<CborMajor, uint64_t>>(b.error());
        arg = b.value();
    } else if (ai == 25) {
        auto b = read_bytes_be(2);
        if (!b) return Err<std::pair<CborMajor, uint64_t>>(b.error());
        arg = b.value();
    } else if (ai == 26) {
        auto b = read_bytes_be(4);
        if (!b) return Err<std::pair<CborMajor, uint64_t>>(b.error());
        arg = b.value();
    } else if (ai == 27) {
        auto b = read_bytes_be(8);
        if (!b) return Err<std::pair<CborMajor, uint64_t>>(b.error());
        arg = b.value();
    } else {
        return Err<std::pair<CborMajor, uint64_t>>(ErrorCode::DecodingError, "CBOR: reserved additional info");
    }
    return Ok(std::make_pair(major, arg));
}

Result<CborMajor> CborReader::peek_major() const {
    if (pos_ >= data_.size()) {
        return Err<CborMajor>(ErrorCode::DecodingError, "CBOR: unexpected end");
    }
    return Ok(static_cast<CborMajor>(data_[pos_] >> 5));
}

Result<size_t> CborReader::read_array_header() {
    auto h = read_head();
    if (!h) return Err<size_t>(h.error());
    if (h.value().first != CborMajor::Array) {
        return Err<size_t>(ErrorCode::DecodingError, "CBOR: expected array");
    }
    return Ok<size_t>(h.value().second);
}

Result<size_t> CborReader::read_map_header() {
    auto h = read_head();
    if (!h) return Err<size_t>(h.error());
    if (h.value().first != CborMajor::Map) {
        return Err<size_t>(ErrorCode::DecodingError, "CBOR: expected map");
    }
    return Ok<size_t>(h.value().second);
}

Result<uint64_t> CborReader::read_uint() {
    auto h = read_head();
    if (!h) return Err<uint64_t>(h.error());
    if (h.value().first != CborMajor::Unsigned) {
        return Err<uint64_t>(ErrorCode::DecodingError, "CBOR: expected unsigned");
    }
    return Ok(h.value().second);
}

Result<int64_t> CborReader::read_int() {
    auto h = read_head();
    if (!h) return Err<int64_t>(h.error());
    if (h.value().first == CborMajor::Unsigned) {
        return Ok(static_cast<int64_t>(h.value().second));
    }
    if (h.value().first == CborMajor::Negative) {
        return Ok(-1 - static_cast<int64_t>(h.value().second));
    }
    return Err<int64_t>(ErrorCode::DecodingError, "CBOR: expected integer");
}

Result<bool> CborReader::read_bool() {
    if (pos_ >= data_.size()) {
        return Err<bool>(ErrorCode::DecodingError, "CBOR: unexpected end");
    }
    const uint8_t b = data_[pos_];
    if (b == ((static_cast<uint8_t>(CborMajor::Simple) << 5) | kSimpleFalse)) {
        ++pos_;
        return Ok(false);
    }
    if (b == ((static_cast<uint8_t>(CborMajor::Simple) << 5) | kSimpleTrue)) {
        ++pos_;
        return Ok(true);
    }
    return Err<bool>(ErrorCode::DecodingError, "CBOR: expected boolean");
}

Result<double> CborReader::read_double() {
    auto major = peek_major();
    if (!major) return Err<double>(major.error());

    if (major.value() == CborMajor::Unsigned || major.value() == CborMajor::Negative) {
        auto i = read_int();
        if (!i) return Err<double>(i.error());
        return Ok(static_cast<double>(i.value()));
    }

    if (major.value() != CborMajor::Simple) {
        return Err<double>(ErrorCode::DecodingError, "CBOR: expected number");
    }
    if (pos_ >= data_.size()) {
        return Err<double>(ErrorCode::DecodingError, "CBOR: unexpected end");
    }
    const uint8_t ai = data_[pos_++] & 0x1F;
    if (ai == kAiFloat64) {
        auto b = read_bytes_be(8);
        if (!b) return Err<double>(b.error());
        double out;
        uint64_t bits = b.value();
        std::memcpy(&out, &bits, sizeof(out));
        return Ok(out);
    }
    if (ai == kAiFloat32) {
        auto b = read_bytes_be(4);
        if (!b) return Err<double>(b.error());
        float out;
        uint32_t bits = static_cast<uint32_t>(b.value());
        std::memcpy(&out, &bits, sizeof(out));
        return Ok(static_cast<double>(out));
    }
    if (ai == kAiFloat16) {
        auto b = read_bytes_be(2);
        if (!b) return Err<double>(b.error());
        // Half-precision → double (RFC 8949 Appendix D).
        const uint32_t half = static_cast<uint32_t>(b.value());
        const uint32_t exp = (half >> 10) & 0x1F;
        const uint32_t mant = half & 0x3FF;
        double val;
        if (exp == 0) {
            val = std::ldexp(static_cast<double>(mant), -24);
        } else if (exp != 31) {
            val = std::ldexp(static_cast<double>(mant + 1024), static_cast<int>(exp) - 25);
        } else {
            val = (mant == 0) ? std::numeric_limits<double>::infinity()
                              : std::numeric_limits<double>::quiet_NaN();
        }
        return Ok((half & 0x8000) ? -val : val);
    }
    return Err<double>(ErrorCode::DecodingError, "CBOR: expected float");
}

Result<std::string> CborReader::read_text() {
    auto h = read_head();
    if (!h) return Err<std::string>(h.error());
    if (h.value().first != CborMajor::Text) {
        return Err<std::string>(ErrorCode::DecodingError, "CBOR: expected text");
    }
    const size_t len = h.value().second;
    if (pos_ + len > data_.size()) {
        return Err<std::string>(ErrorCode::DecodingError, "CBOR: truncated text");
    }
    std::string s(reinterpret_cast<const char*>(data_.data() + pos_), len);
    pos_ += len;
    return Ok(std::move(s));
}

Result<std::vector<uint8_t>> CborReader::read_bytes() {
    auto h = read_head();
    if (!h) return Err<std::vector<uint8_t>>(h.error());
    if (h.value().first != CborMajor::Bytes) {
        return Err<std::vector<uint8_t>>(ErrorCode::DecodingError, "CBOR: expected bytes");
    }
    const size_t len = h.value().second;
    if (pos_ + len > data_.size()) {
        return Err<std::vector<uint8_t>>(ErrorCode::DecodingError, "CBOR: truncated bytes");
    }
    std::vector<uint8_t> out(data_.data() + pos_, data_.data() + pos_ + len);
    pos_ += len;
    return Ok(std::move(out));
}

}  // namespace lwm2m::codec
