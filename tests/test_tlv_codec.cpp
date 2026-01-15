// Ferriot - TLV Codec Unit Tests

#include <gtest/gtest.h>

#include "lwm2m/codec/tlv.hpp"

namespace lwm2m::codec {
namespace {

class TlvEncoderTest : public ::testing::Test {
protected:
    TlvEncoder encoder;
};

TEST_F(TlvEncoderTest, EncodeBoolean) {
    auto result = encoder.encode_resource(ResourceId{0}, true);
    ASSERT_TRUE(result.is_ok());

    auto& data = result.value();
    EXPECT_FALSE(data.empty());
    // Type byte should indicate Resource type (0b11xxxxxx)
    EXPECT_EQ((data[0] >> 6) & 0x03, 0x03);
}

TEST_F(TlvEncoderTest, EncodeInteger) {
    auto result = encoder.encode_resource(ResourceId{1}, int64_t{42});
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeLargeInteger) {
    auto result = encoder.encode_resource(ResourceId{1}, int64_t{1234567890});
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeNegativeInteger) {
    auto result = encoder.encode_resource(ResourceId{1}, int64_t{-42});
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeFloat) {
    auto result = encoder.encode_resource(ResourceId{2}, 3.14159);
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeString) {
    auto result = encoder.encode_resource(ResourceId{3}, std::string{"Hello, LWM2M!"});
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeOpaque) {
    std::vector<uint8_t> opaque = {0xDE, 0xAD, 0xBE, 0xEF};
    auto result = encoder.encode_resource(ResourceId{4}, opaque);
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeTime) {
    auto time = std::chrono::system_clock::from_time_t(1609459200);  // 2021-01-01 00:00:00 UTC
    auto result = encoder.encode_resource(ResourceId{5}, time);
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeObjectLink) {
    auto link = ObjectPath::instance(ObjectId{3}, InstanceId{0});
    auto result = encoder.encode_resource(ResourceId{6}, link);
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().empty());
}

TEST_F(TlvEncoderTest, EncodeRecordSmallId) {
    auto record = encoder.encode_record(TlvType::Resource, 5, {});
    EXPECT_GE(record.size(), 2u);  // At least type byte + id byte
    EXPECT_EQ((record[0] >> 5) & 0x01, 0);  // 8-bit ID flag
}

TEST_F(TlvEncoderTest, EncodeRecordLargeId) {
    auto record = encoder.encode_record(TlvType::Resource, 1000, {});
    EXPECT_GE(record.size(), 3u);  // Type byte + 2 id bytes
    EXPECT_EQ((record[0] >> 5) & 0x01, 1);  // 16-bit ID flag
}

class TlvDecoderTest : public ::testing::Test {
protected:
    TlvDecoder decoder;
    TlvEncoder encoder;
};

TEST_F(TlvDecoderTest, DecodeBoolean) {
    std::vector<uint8_t> data = {1};  // true
    auto result = decoder.decode_resource(data, ResourceType::Boolean);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(std::get<bool>(result.value()));
}

TEST_F(TlvDecoderTest, DecodeInteger) {
    std::vector<uint8_t> data = {0x2A};  // 42
    auto result = decoder.decode_resource(data, ResourceType::Integer);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 42);
}

TEST_F(TlvDecoderTest, DecodeNegativeInteger) {
    std::vector<uint8_t> data = {0xD6};  // -42 (as signed byte)
    auto result = decoder.decode_resource(data, ResourceType::Integer);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), -42);
}

TEST_F(TlvDecoderTest, DecodeString) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    auto result = decoder.decode_resource(data, ResourceType::String);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<std::string>(result.value()), "Hello");
}

TEST_F(TlvDecoderTest, DecodeOpaque) {
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    auto result = decoder.decode_resource(data, ResourceType::Opaque);
    ASSERT_TRUE(result.is_ok());
    auto& opaque = std::get<std::vector<uint8_t>>(result.value());
    EXPECT_EQ(opaque, data);
}

TEST_F(TlvDecoderTest, DecodeObjectLink) {
    std::vector<uint8_t> data = {0x00, 0x03, 0x00, 0x00};  // /3/0
    auto result = decoder.decode_resource(data, ResourceType::ObjectLink);
    ASSERT_TRUE(result.is_ok());
    auto& link = std::get<ObjectPath>(result.value());
    EXPECT_EQ(link.object_id.value, 3);
    EXPECT_EQ(link.instance_id->value, 0);
}

TEST_F(TlvDecoderTest, RoundTripInteger) {
    auto encoded = encoder.encode_resource(ResourceId{0}, int64_t{12345});
    ASSERT_TRUE(encoded.is_ok());

    auto decoded = decoder.decode(encoded.value());
    ASSERT_TRUE(decoded.is_ok());
    ASSERT_EQ(decoded.value().size(), 1u);

    auto& record = decoded.value()[0];
    EXPECT_EQ(record.id, 0);
    EXPECT_EQ(record.type, TlvType::Resource);

    auto value = decoder.decode_resource(record.value, ResourceType::Integer);
    ASSERT_TRUE(value.is_ok());
    EXPECT_EQ(std::get<int64_t>(value.value()), 12345);
}

TEST_F(TlvDecoderTest, RoundTripString) {
    std::string original = "Test String";
    auto encoded = encoder.encode_resource(ResourceId{1}, original);
    ASSERT_TRUE(encoded.is_ok());

    auto decoded = decoder.decode(encoded.value());
    ASSERT_TRUE(decoded.is_ok());
    ASSERT_EQ(decoded.value().size(), 1u);

    auto& record = decoded.value()[0];
    auto value = decoder.decode_resource(record.value, ResourceType::String);
    ASSERT_TRUE(value.is_ok());
    EXPECT_EQ(std::get<std::string>(value.value()), original);
}

TEST_F(TlvDecoderTest, DecodeEmptyData) {
    auto result = decoder.decode({});
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(TlvDecoderTest, DecodeTruncatedData) {
    std::vector<uint8_t> truncated = {0xC1};  // Type byte only, missing ID and value
    auto result = decoder.decode(truncated);
    EXPECT_TRUE(result.is_err());
}

}  // namespace
}  // namespace lwm2m::codec
