// Ferriot - SenML CBOR codec (RFC 8428 + RFC 8949, content format 112).

#include "lwm2m/codec/senml_cbor.hpp"
#include "lwm2m/codec/senml.hpp"
#include "lwm2m/codec/cbor.hpp"

namespace lwm2m::codec {

namespace {

// SenML CBOR integer labels (RFC 8428 §6). vlo has no assigned integer label,
// so the Object Link travels under the text key "vlo".
constexpr int64_t kBaseName = -2;
constexpr int64_t kName = 0;
constexpr int64_t kValue = 2;
constexpr int64_t kStringValue = 3;
constexpr int64_t kBoolValue = 4;
constexpr int64_t kTime = 6;
constexpr int64_t kDataValue = 8;

size_t field_count(const ResourceValue& v, bool with_base) {
    (void)v;
    return with_base ? 3 : 2;
}

void write_value(CborWriter& w, const ResourceValue& v) {
    std::visit([&](const auto& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, bool>) {
            w.write_int(kBoolValue);
            w.write_bool(x);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            w.write_int(kValue);
            w.write_int(x);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            w.write_int(kValue);
            w.write_uint(x);
        } else if constexpr (std::is_same_v<T, double>) {
            w.write_int(kValue);
            w.write_double(x);
        } else if constexpr (std::is_same_v<T, std::string>) {
            w.write_int(kStringValue);
            w.write_text(x);
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            w.write_int(kDataValue);
            w.write_bytes(x);
        } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
            w.write_int(kValue);
            w.write_int(std::chrono::duration_cast<std::chrono::seconds>(
                x.time_since_epoch()).count());
        } else if constexpr (std::is_same_v<T, ObjectPath>) {
            w.write_text("vlo");
            w.write_text(std::to_string(x.object_id.value) + ":" +
                         std::to_string(x.instance_id ? x.instance_id->value : 0));
        } else {
            w.write_int(kStringValue);
            w.write_text("");
        }
    }, v);
}

Result<void> skip_item(CborReader& r) {
    auto major = r.peek_major();
    if (!major) return Err<void>(major.error());
    switch (major.value()) {
        case CborMajor::Unsigned:
        case CborMajor::Negative: { auto v = r.read_int(); return v ? Ok() : Err<void>(v.error()); }
        case CborMajor::Text:     { auto v = r.read_text(); return v ? Ok() : Err<void>(v.error()); }
        case CborMajor::Bytes:    { auto v = r.read_bytes(); return v ? Ok() : Err<void>(v.error()); }
        case CborMajor::Simple:   { auto v = r.read_double(); return v ? Ok() : Err<void>(v.error()); }
        default:
            return Err<void>(ErrorCode::DecodingError, "SenML CBOR: unsupported item to skip");
    }
}

Result<void> read_numeric(CborReader& r, ResourceValue& out) {
    auto major = r.peek_major();
    if (!major) return Err<void>(major.error());
    if (major.value() == CborMajor::Unsigned || major.value() == CborMajor::Negative) {
        auto i = r.read_int();
        if (!i) return Err<void>(i.error());
        out = ResourceValue{i.value()};
    } else {
        auto d = r.read_double();
        if (!d) return Err<void>(d.error());
        out = ResourceValue{d.value()};
    }
    return Ok();
}

}  // namespace

Result<std::vector<uint8_t>> SenmlCborCodec::encode_read(
    const ObjectPath& base, const std::vector<ReadEntry>& entries) {
    const SenmlPayload payload = build_senml_payload(base, entries);

    CborWriter w;
    if (payload.items.empty()) {
        w.array_header(1);
        w.map_header(1);
        w.write_int(kBaseName);
        w.write_text(payload.base_name);
        return Ok(w.take());
    }

    w.array_header(payload.items.size());
    bool first = true;
    for (const auto& item : payload.items) {
        w.map_header(field_count(item.value, first));
        if (first) {
            w.write_int(kBaseName);
            w.write_text(payload.base_name);
        }
        w.write_int(kName);
        w.write_text(item.name);
        write_value(w, item.value);
        first = false;
    }
    return Ok(w.take());
}

Result<std::vector<ResourceRecord>> SenmlCborCodec::decode_write(
    const std::vector<uint8_t>& data, const TypeResolver& type_of) {
    CborReader r(data);
    auto count = r.read_array_header();
    if (!count) return Err<std::vector<ResourceRecord>>(count.error());

    std::vector<SenmlInRecord> records;
    records.reserve(count.value());

    for (size_t i = 0; i < count.value(); ++i) {
        auto pairs = r.read_map_header();
        if (!pairs) return Err<std::vector<ResourceRecord>>(pairs.error());

        SenmlInRecord rec;
        bool value_set = false;
        for (size_t p = 0; p < pairs.value(); ++p) {
            auto key_major = r.peek_major();
            if (!key_major) return Err<std::vector<ResourceRecord>>(key_major.error());

            if (key_major.value() == CborMajor::Text) {
                auto key = r.read_text();
                if (!key) return Err<std::vector<ResourceRecord>>(key.error());
                if (key.value() == "vlo") {
                    auto s = r.read_text();
                    if (!s) return Err<std::vector<ResourceRecord>>(s.error());
                    auto link = parse_objlink(s.value());
                    if (!link) return Err<std::vector<ResourceRecord>>(link.error());
                    rec.value = ResourceValue{link.value()};
                    value_set = true;
                } else if (key.value() == "bn") {
                    auto s = r.read_text();
                    if (!s) return Err<std::vector<ResourceRecord>>(s.error());
                    rec.bn = std::move(s.value());
                } else if (key.value() == "n") {
                    auto s = r.read_text();
                    if (!s) return Err<std::vector<ResourceRecord>>(s.error());
                    rec.n = std::move(s.value());
                } else {
                    if (auto e = skip_item(r); !e) return Err<std::vector<ResourceRecord>>(e.error());
                }
                continue;
            }

            auto key = r.read_int();
            if (!key) return Err<std::vector<ResourceRecord>>(key.error());
            switch (key.value()) {
                case kBaseName: {
                    auto s = r.read_text();
                    if (!s) return Err<std::vector<ResourceRecord>>(s.error());
                    rec.bn = std::move(s.value());
                    break;
                }
                case kName: {
                    auto s = r.read_text();
                    if (!s) return Err<std::vector<ResourceRecord>>(s.error());
                    rec.n = std::move(s.value());
                    break;
                }
                case kValue: {
                    if (auto e = read_numeric(r, rec.value); !e) return Err<std::vector<ResourceRecord>>(e.error());
                    value_set = true;
                    break;
                }
                case kStringValue: {
                    auto s = r.read_text();
                    if (!s) return Err<std::vector<ResourceRecord>>(s.error());
                    rec.value = ResourceValue{std::move(s.value())};
                    value_set = true;
                    break;
                }
                case kBoolValue: {
                    auto b = r.read_bool();
                    if (!b) return Err<std::vector<ResourceRecord>>(b.error());
                    rec.value = ResourceValue{b.value()};
                    value_set = true;
                    break;
                }
                case kDataValue: {
                    auto b = r.read_bytes();
                    if (!b) return Err<std::vector<ResourceRecord>>(b.error());
                    rec.value = ResourceValue{std::move(b.value())};
                    value_set = true;
                    break;
                }
                case kTime: {
                    ResourceValue ignored;
                    if (auto e = read_numeric(r, ignored); !e) return Err<std::vector<ResourceRecord>>(e.error());
                    break;
                }
                default:
                    if (auto e = skip_item(r); !e) return Err<std::vector<ResourceRecord>>(e.error());
                    break;
            }
        }
        if (!value_set) {
            return Err<std::vector<ResourceRecord>>(ErrorCode::DecodingError, "SenML CBOR: record has no value");
        }
        records.push_back(std::move(rec));
    }

    return resolve_senml(records, type_of);
}

}  // namespace lwm2m::codec
