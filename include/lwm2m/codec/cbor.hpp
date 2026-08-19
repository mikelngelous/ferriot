#pragma once

// Ferriot - Minimal CBOR (RFC 8949) reader/writer, subset used by SenML.

#include "../result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lwm2m::codec {

enum class CborMajor : uint8_t {
    Unsigned = 0,
    Negative = 1,
    Bytes = 2,
    Text = 3,
    Array = 4,
    Map = 5,
    Simple = 7,
};

class CborWriter {
public:
    void array_header(size_t count) { write_head(CborMajor::Array, count); }
    void map_header(size_t count) { write_head(CborMajor::Map, count); }

    void write_uint(uint64_t v) { write_head(CborMajor::Unsigned, v); }
    void write_int(int64_t v);
    void write_bool(bool v);
    void write_double(double v);
    void write_text(std::string_view s);
    void write_bytes(const std::vector<uint8_t>& b);

    [[nodiscard]] const std::vector<uint8_t>& data() const noexcept { return buf_; }
    [[nodiscard]] std::vector<uint8_t> take() noexcept { return std::move(buf_); }

private:
    void write_head(CborMajor major, uint64_t arg);

    std::vector<uint8_t> buf_;
};

class CborReader {
public:
    explicit CborReader(const std::vector<uint8_t>& data) : data_(data) {}

    [[nodiscard]] Result<CborMajor> peek_major() const;

    [[nodiscard]] Result<size_t> read_array_header();
    [[nodiscard]] Result<size_t> read_map_header();

    [[nodiscard]] Result<uint64_t> read_uint();
    [[nodiscard]] Result<int64_t> read_int();
    [[nodiscard]] Result<bool> read_bool();
    [[nodiscard]] Result<double> read_double();
    [[nodiscard]] Result<std::string> read_text();
    [[nodiscard]] Result<std::vector<uint8_t>> read_bytes();

    [[nodiscard]] bool at_end() const noexcept { return pos_ >= data_.size(); }

private:
    Result<std::pair<CborMajor, uint64_t>> read_head();
    Result<uint64_t> read_bytes_be(size_t n);

    const std::vector<uint8_t>& data_;
    size_t pos_{0};
};

}  // namespace lwm2m::codec
