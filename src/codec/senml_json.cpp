// Ferriot - SenML JSON codec (RFC 8428, content format 110).

#include "lwm2m/codec/senml_json.hpp"
#include "lwm2m/codec/senml.hpp"

#include <cstdio>
#include <cstdlib>

namespace lwm2m::codec {

namespace {

void append_escaped(std::string& out, const std::string& s) {
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

void append_value(std::string& out, const ResourceValue& v) {
    std::visit([&](const auto& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, bool>) {
            out += "\"vb\":";
            out += x ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int64_t>) {
            out += "\"v\":" + std::to_string(x);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            out += "\"v\":" + std::to_string(x);
        } else if constexpr (std::is_same_v<T, double>) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.17g", x);
            out += "\"v\":";
            out += buf;
        } else if constexpr (std::is_same_v<T, std::string>) {
            out += "\"vs\":";
            append_escaped(out, x);
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            out += "\"vd\":";
            append_escaped(out, base64url_encode(x));
        } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
            const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                                  x.time_since_epoch()).count();
            out += "\"v\":" + std::to_string(secs);
        } else if constexpr (std::is_same_v<T, ObjectPath>) {
            out += "\"vlo\":\"" + std::to_string(x.object_id.value) + ":" +
                   std::to_string(x.instance_id ? x.instance_id->value : 0) + "\"";
        } else {
            out += "\"vs\":\"\"";
        }
    }, v);
}

// Minimal JSON parser for the SenML subset (array of flat objects).
class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s) {}

    Result<std::vector<SenmlInRecord>> parse() {
        skip_ws();
        if (!consume('[')) {
            return Err<std::vector<SenmlInRecord>>(ErrorCode::DecodingError, "SenML JSON: expected '['");
        }
        std::vector<SenmlInRecord> records;
        skip_ws();
        if (consume(']')) return Ok(std::move(records));
        while (true) {
            auto rec = parse_record();
            if (!rec) return Err<std::vector<SenmlInRecord>>(rec.error());
            records.push_back(std::move(rec.value()));
            skip_ws();
            if (consume(',')) { skip_ws(); continue; }
            if (consume(']')) break;
            return Err<std::vector<SenmlInRecord>>(ErrorCode::DecodingError, "SenML JSON: expected ',' or ']'");
        }
        return Ok(std::move(records));
    }

private:
    Result<SenmlInRecord> parse_record() {
        if (!consume('{')) {
            return Err<SenmlInRecord>(ErrorCode::DecodingError, "SenML JSON: expected '{'");
        }
        SenmlInRecord rec;
        bool value_set = false;
        skip_ws();
        if (consume('}')) return Ok(std::move(rec));
        while (true) {
            skip_ws();
            auto key = parse_string();
            if (!key) return Err<SenmlInRecord>(key.error());
            skip_ws();
            if (!consume(':')) {
                return Err<SenmlInRecord>(ErrorCode::DecodingError, "SenML JSON: expected ':'");
            }
            skip_ws();

            const std::string& k = key.value();
            if (k == "bn") {
                auto v = parse_string();
                if (!v) return Err<SenmlInRecord>(v.error());
                rec.bn = std::move(v.value());
            } else if (k == "n") {
                auto v = parse_string();
                if (!v) return Err<SenmlInRecord>(v.error());
                rec.n = std::move(v.value());
            } else if (k == "vs") {
                auto v = parse_string();
                if (!v) return Err<SenmlInRecord>(v.error());
                rec.value = ResourceValue{std::move(v.value())};
                value_set = true;
            } else if (k == "vb") {
                auto v = parse_bool();
                if (!v) return Err<SenmlInRecord>(v.error());
                rec.value = ResourceValue{v.value()};
                value_set = true;
            } else if (k == "vd") {
                auto v = parse_string();
                if (!v) return Err<SenmlInRecord>(v.error());
                auto bytes = base64url_decode(v.value());
                if (!bytes) return Err<SenmlInRecord>(bytes.error());
                rec.value = ResourceValue{std::move(bytes.value())};
                value_set = true;
            } else if (k == "vlo") {
                auto v = parse_string();
                if (!v) return Err<SenmlInRecord>(v.error());
                auto link = parse_objlink(v.value());
                if (!link) return Err<SenmlInRecord>(link.error());
                rec.value = ResourceValue{link.value()};
                value_set = true;
            } else if (k == "v") {
                auto v = parse_number(rec.value);
                if (!v) return Err<SenmlInRecord>(v.error());
                value_set = true;
            } else {
                if (auto e = skip_value(); !e) return Err<SenmlInRecord>(e.error());
            }

            skip_ws();
            if (consume(',')) continue;
            if (consume('}')) break;
            return Err<SenmlInRecord>(ErrorCode::DecodingError, "SenML JSON: expected ',' or '}'");
        }
        if (!value_set) {
            return Err<SenmlInRecord>(ErrorCode::DecodingError, "SenML JSON: record has no value");
        }
        return Ok(std::move(rec));
    }

    Result<std::string> parse_string() {
        if (!consume('"')) {
            return Err<std::string>(ErrorCode::DecodingError, "SenML JSON: expected string");
        }
        std::string out;
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return Ok(std::move(out));
            if (c == '\\') {
                if (pos_ >= s_.size()) break;
                char e = s_[pos_++];
                switch (e) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > s_.size()) {
                            return Err<std::string>(ErrorCode::DecodingError, "SenML JSON: bad \\u");
                        }
                        const uint32_t cp = static_cast<uint32_t>(
                            std::strtoul(s_.substr(pos_, 4).c_str(), nullptr, 16));
                        pos_ += 4;
                        append_utf8(out, cp);
                        break;
                    }
                    default:
                        return Err<std::string>(ErrorCode::DecodingError, "SenML JSON: bad escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return Err<std::string>(ErrorCode::DecodingError, "SenML JSON: unterminated string");
    }

    Result<bool> parse_bool() {
        if (s_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Ok(true); }
        if (s_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Ok(false); }
        return Err<bool>(ErrorCode::DecodingError, "SenML JSON: expected boolean");
    }

    Result<bool> parse_number(ResourceValue& out) {
        const size_t start = pos_;
        bool is_float = false;
        if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) ++pos_;
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if ((c >= '0' && c <= '9')) { ++pos_; continue; }
            if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') { is_float = true; ++pos_; continue; }
            break;
        }
        if (pos_ == start) {
            return Err<bool>(ErrorCode::DecodingError, "SenML JSON: expected number");
        }
        const std::string tok = s_.substr(start, pos_ - start);
        if (is_float) {
            out = ResourceValue{std::strtod(tok.c_str(), nullptr)};
        } else {
            out = ResourceValue{static_cast<int64_t>(std::strtoll(tok.c_str(), nullptr, 10))};
        }
        return Ok(true);
    }

    Result<bool> skip_value() {
        skip_ws();
        if (pos_ >= s_.size()) return Err<bool>(ErrorCode::DecodingError, "SenML JSON: truncated");
        char c = s_[pos_];
        if (c == '"') { auto s = parse_string(); return s ? Ok(true) : Err<bool>(s.error()); }
        if (c == 't' || c == 'f') { auto b = parse_bool(); return b ? Ok(true) : Err<bool>(b.error()); }
        if (c == 'n') { pos_ += 4; return Ok(true); }
        ResourceValue tmp;
        return parse_number(tmp);
    }

    static void append_utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    void skip_ws() {
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos_; } else { break; }
        }
    }
    bool consume(char c) {
        if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    const std::string& s_;
    size_t pos_{0};
};

}  // namespace

Result<std::vector<uint8_t>> SenmlJsonCodec::encode_read(
    const ObjectPath& base, const std::vector<ReadEntry>& entries) {
    const SenmlPayload payload = build_senml_payload(base, entries);

    std::string out = "[";
    bool first = true;
    for (const auto& item : payload.items) {
        if (!first) out.push_back(',');
        out.push_back('{');
        if (first) {
            out += "\"bn\":";
            std::string bn;
            append_escaped(bn, payload.base_name);
            out += bn;
            out.push_back(',');
        }
        out += "\"n\":";
        std::string n;
        append_escaped(n, item.name);
        out += n;
        out.push_back(',');
        append_value(out, item.value);
        out.push_back('}');
        first = false;
    }
    if (payload.items.empty()) {
        out += "{\"bn\":";
        std::string bn;
        append_escaped(bn, payload.base_name);
        out += bn;
        out.push_back('}');
    }
    out.push_back(']');

    return Ok(std::vector<uint8_t>(out.begin(), out.end()));
}

Result<std::vector<ResourceRecord>> SenmlJsonCodec::decode_write(
    const std::vector<uint8_t>& data, const TypeResolver& type_of) {
    const std::string text(data.begin(), data.end());
    JsonParser parser(text);
    auto records = parser.parse();
    if (!records) return Err<std::vector<ResourceRecord>>(records.error());
    return resolve_senml(records.value(), type_of);
}

}  // namespace lwm2m::codec
