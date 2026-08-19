// Ferriot - CBOR reader/writer round-trip tests (RFC 8949 subset).

#include <gtest/gtest.h>

#include "lwm2m/codec/cbor.hpp"

#include <limits>

using namespace lwm2m::codec;

namespace {

TEST(CborWriter, CanonicalUintEncodings) {
    {
        CborWriter w;
        w.write_uint(0);
        EXPECT_EQ(w.data(), (std::vector<uint8_t>{0x00}));
    }
    {
        CborWriter w;
        w.write_uint(23);
        EXPECT_EQ(w.data(), (std::vector<uint8_t>{0x17}));
    }
    {
        CborWriter w;
        w.write_uint(24);
        EXPECT_EQ(w.data(), (std::vector<uint8_t>{0x18, 0x18}));
    }
    {
        CborWriter w;
        w.write_uint(1000);
        EXPECT_EQ(w.data(), (std::vector<uint8_t>{0x19, 0x03, 0xE8}));
    }
    {
        CborWriter w;
        w.write_uint(1000000);
        EXPECT_EQ(w.data(), (std::vector<uint8_t>{0x1A, 0x00, 0x0F, 0x42, 0x40}));
    }
}

TEST(CborWriter, NegativeIntEncoding) {
    CborWriter w;
    w.write_int(-500);
    EXPECT_EQ(w.data(), (std::vector<uint8_t>{0x39, 0x01, 0xF3}));
}

TEST(CborRoundTrip, Integers) {
    const int64_t values[] = {0, 1, -1, 23, 24, -24, 255, 256, -256,
                              1000000, -1000000,
                              std::numeric_limits<int32_t>::min()};
    for (int64_t v : values) {
        CborWriter w;
        w.write_int(v);
        CborReader r(w.data());
        auto got = r.read_int();
        ASSERT_TRUE(got) << "value " << v;
        EXPECT_EQ(got.value(), v);
        EXPECT_TRUE(r.at_end());
    }
}

TEST(CborRoundTrip, Doubles) {
    const double values[] = {0.0, 1.5, -1.5, 3.14159265358979,
                             1e300, -1e-300};
    for (double v : values) {
        CborWriter w;
        w.write_double(v);
        CborReader r(w.data());
        auto got = r.read_double();
        ASSERT_TRUE(got);
        EXPECT_DOUBLE_EQ(got.value(), v);
    }
}

TEST(CborRoundTrip, DoubleReaderAcceptsIntEncoding) {
    CborWriter w;
    w.write_int(42);
    CborReader r(w.data());
    auto got = r.read_double();
    ASSERT_TRUE(got);
    EXPECT_DOUBLE_EQ(got.value(), 42.0);
}

TEST(CborRoundTrip, Booleans) {
    for (bool v : {true, false}) {
        CborWriter w;
        w.write_bool(v);
        CborReader r(w.data());
        auto got = r.read_bool();
        ASSERT_TRUE(got);
        EXPECT_EQ(got.value(), v);
    }
}

TEST(CborRoundTrip, Text) {
    CborWriter w;
    w.write_text("hello \xC3\xA9");
    CborReader r(w.data());
    auto got = r.read_text();
    ASSERT_TRUE(got);
    EXPECT_EQ(got.value(), "hello \xC3\xA9");
}

TEST(CborRoundTrip, Bytes) {
    std::vector<uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF, 0x00};
    CborWriter w;
    w.write_bytes(payload);
    CborReader r(w.data());
    auto got = r.read_bytes();
    ASSERT_TRUE(got);
    EXPECT_EQ(got.value(), payload);
}

TEST(CborRoundTrip, ArrayOfMaps) {
    CborWriter w;
    w.array_header(2);
    w.map_header(2);
    w.write_int(-2);
    w.write_text("/3/0/");
    w.write_int(0);
    w.write_text("9");
    w.map_header(1);
    w.write_int(2);
    w.write_int(82);

    CborReader r(w.data());
    auto n = r.read_array_header();
    ASSERT_TRUE(n);
    EXPECT_EQ(n.value(), 2u);

    auto m0 = r.read_map_header();
    ASSERT_TRUE(m0);
    EXPECT_EQ(m0.value(), 2u);
    EXPECT_EQ(r.read_int().value(), -2);
    EXPECT_EQ(r.read_text().value(), "/3/0/");
    EXPECT_EQ(r.read_int().value(), 0);
    EXPECT_EQ(r.read_text().value(), "9");

    auto m1 = r.read_map_header();
    ASSERT_TRUE(m1);
    EXPECT_EQ(m1.value(), 1u);
    EXPECT_EQ(r.read_int().value(), 2);
    EXPECT_EQ(r.read_int().value(), 82);
    EXPECT_TRUE(r.at_end());
}

TEST(CborReader, PeekMajor) {
    CborWriter w;
    w.write_text("x");
    CborReader r(w.data());
    auto m = r.peek_major();
    ASSERT_TRUE(m);
    EXPECT_EQ(m.value(), CborMajor::Text);
}

TEST(CborReader, TruncatedInputFails) {
    std::vector<uint8_t> truncated{0x19, 0x03};
    CborReader r(truncated);
    EXPECT_FALSE(r.read_uint());
}

TEST(CborReader, TypeMismatchFails) {
    CborWriter w;
    w.write_text("nope");
    CborReader r(w.data());
    EXPECT_FALSE(r.read_int());
}

}  // namespace
