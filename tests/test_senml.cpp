// Ferriot - SenML JSON/CBOR codec round-trip tests (RFC 8428).

#include <gtest/gtest.h>

#include "lwm2m/codec/codec.hpp"
#include "lwm2m/codec/senml.hpp"

#include <string>

using namespace lwm2m;
using namespace lwm2m::codec;

namespace {

std::optional<ResourceType> device_types(ResourceId rid) {
    switch (rid.value) {
        case 1:  return ResourceType::Integer;
        case 2:  return ResourceType::UnsignedInteger;
        case 3:  return ResourceType::Float;
        case 4:  return ResourceType::String;
        case 5:  return ResourceType::Boolean;
        case 6:  return ResourceType::Opaque;
        case 7:  return ResourceType::Time;
        case 8:  return ResourceType::ObjectLink;
        default: return std::nullopt;
    }
}

std::map<uint16_t, ResourceValue> round_trip(transport::ContentFormat fmt,
                                             const std::vector<ReadEntry>& entries) {
    auto codec = select_codec(fmt);
    EXPECT_EQ(codec->format(), fmt);
    ObjectPath base;
    base.object_id = ObjectId{3};
    base.instance_id = InstanceId{0};

    auto encoded = codec->encode_read(base, entries);
    EXPECT_TRUE(encoded);

    auto records = codec->decode_write(encoded.value(), device_types);
    EXPECT_TRUE(records) << "decode failed";

    std::map<uint16_t, ResourceValue> out;
    if (records) {
        for (const auto& r : records.value()) {
            out[r.rid.value] = r.value;
        }
    }
    return out;
}

ReadEntry entry(uint16_t rid, ResourceValue v) {
    return ReadEntry{InstanceId{0}, ResourceId{rid}, std::move(v)};
}

class SenmlCodecParam : public ::testing::TestWithParam<transport::ContentFormat> {};

TEST_P(SenmlCodecParam, AllScalarTypesRoundTrip) {
    const auto tp = std::chrono::system_clock::time_point{std::chrono::seconds{1700000000}};
    ObjectPath link;
    link.object_id = ObjectId{3};
    link.instance_id = InstanceId{0};

    std::vector<ReadEntry> entries = {
        entry(1, ResourceValue{int64_t{-42}}),
        entry(2, ResourceValue{uint64_t{4200000000ULL}}),
        entry(3, ResourceValue{double{3.5}}),
        entry(4, ResourceValue{std::string{"hello"}}),
        entry(5, ResourceValue{true}),
        entry(6, ResourceValue{std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}}),
        entry(7, ResourceValue{tp}),
        entry(8, ResourceValue{link}),
    };

    auto m = round_trip(GetParam(), entries);
    ASSERT_EQ(m.size(), 8u);

    EXPECT_EQ(std::get<int64_t>(m[1]), -42);
    EXPECT_EQ(std::get<uint64_t>(m[2]), 4200000000ULL);
    EXPECT_DOUBLE_EQ(std::get<double>(m[3]), 3.5);
    EXPECT_EQ(std::get<std::string>(m[4]), "hello");
    EXPECT_EQ(std::get<bool>(m[5]), true);
    EXPECT_EQ(std::get<std::vector<uint8_t>>(m[6]),
              (std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}));
    EXPECT_EQ(std::get<std::chrono::system_clock::time_point>(m[7]), tp);
    const auto& op = std::get<ObjectPath>(m[8]);
    EXPECT_EQ(op.object_id.value, 3);
    ASSERT_TRUE(op.instance_id.has_value());
    EXPECT_EQ(op.instance_id->value, 0);
}

TEST_P(SenmlCodecParam, SingleResourceRoundTrip) {
    auto codec = select_codec(GetParam());
    ObjectPath base;
    base.object_id = ObjectId{3};
    base.instance_id = InstanceId{0};
    base.resource_id = ResourceId{1};

    auto encoded = codec->encode_read(base, {entry(1, ResourceValue{int64_t{99}})});
    ASSERT_TRUE(encoded);
    auto records = codec->decode_write(encoded.value(), device_types);
    ASSERT_TRUE(records);
    ASSERT_EQ(records.value().size(), 1u);
    EXPECT_EQ(records.value()[0].rid.value, 1);
    EXPECT_EQ(std::get<int64_t>(records.value()[0].value), 99);
}

TEST_P(SenmlCodecParam, ObjectLevelNamesCarryInstance) {
    auto codec = select_codec(GetParam());
    ObjectPath base;
    base.object_id = ObjectId{3};

    std::vector<ReadEntry> entries = {
        ReadEntry{InstanceId{0}, ResourceId{1}, ResourceValue{int64_t{10}}},
        ReadEntry{InstanceId{2}, ResourceId{1}, ResourceValue{int64_t{20}}},
    };
    auto encoded = codec->encode_read(base, entries);
    ASSERT_TRUE(encoded);
    auto records = codec->decode_write(encoded.value(), device_types);
    ASSERT_TRUE(records);
    ASSERT_EQ(records.value().size(), 2u);
    EXPECT_EQ(records.value()[0].rid.value, 1);
    EXPECT_EQ(std::get<int64_t>(records.value()[0].value), 10);
    EXPECT_EQ(records.value()[1].rid.value, 1);
    EXPECT_EQ(std::get<int64_t>(records.value()[1].value), 20);
}

INSTANTIATE_TEST_SUITE_P(Formats, SenmlCodecParam,
    ::testing::Values(transport::ContentFormat::SenmlJson,
                      transport::ContentFormat::SenmlCbor));

TEST(SenmlJsonInterop, LeshanStyleWrite) {
    auto codec = select_codec(transport::ContentFormat::SenmlJson);
    const std::string json = R"([{"bn":"/3/0/","n":"1","v":-42},{"n":"5","vb":true},{"n":"4","vs":"abc"}])";
    std::vector<uint8_t> data(json.begin(), json.end());

    auto records = codec->decode_write(data, device_types);
    ASSERT_TRUE(records);
    ASSERT_EQ(records.value().size(), 3u);
    EXPECT_EQ(records.value()[0].rid.value, 1);
    EXPECT_EQ(std::get<int64_t>(records.value()[0].value), -42);
    EXPECT_EQ(records.value()[1].rid.value, 5);
    EXPECT_EQ(std::get<bool>(records.value()[1].value), true);
    EXPECT_EQ(records.value()[2].rid.value, 4);
    EXPECT_EQ(std::get<std::string>(records.value()[2].value), "abc");
}

TEST(SenmlJsonInterop, FloatCoercedToInteger) {
    auto codec = select_codec(transport::ContentFormat::SenmlJson);
    const std::string json = R"([{"bn":"/3/0/","n":"1","v":42.0}])";
    std::vector<uint8_t> data(json.begin(), json.end());
    auto records = codec->decode_write(data, device_types);
    ASSERT_TRUE(records);
    ASSERT_EQ(records.value().size(), 1u);
    EXPECT_EQ(std::get<int64_t>(records.value()[0].value), 42);
}

TEST(SenmlJson, EncodeShapeIsArrayWithBaseName) {
    auto codec = select_codec(transport::ContentFormat::SenmlJson);
    ObjectPath base;
    base.object_id = ObjectId{3};
    base.instance_id = InstanceId{0};
    base.resource_id = ResourceId{9};

    auto encoded = codec->encode_read(base, {ReadEntry{InstanceId{0}, ResourceId{9},
                                                       ResourceValue{int64_t{82}}}});
    ASSERT_TRUE(encoded);
    const std::string s(encoded.value().begin(), encoded.value().end());
    EXPECT_NE(s.find("\"bn\":\"/3/0/\""), std::string::npos) << s;
    EXPECT_NE(s.find("\"n\":\"9\""), std::string::npos) << s;
    EXPECT_NE(s.find("\"v\":82"), std::string::npos) << s;
}

TEST(SenmlCborInterop, DecodesMixedIntAndTextKeys) {
    // Manually assemble: [ {-2:"/3/0/", 0:"1", 2:7} ]
    // CBOR: 81 A3 21 65 2F332F302F 00 61 31 02 07
    std::vector<uint8_t> data = {
        0x81,                         // array(1)
        0xA3,                         // map(3)
        0x21, 0x65, '/', '3', '/', '0', '/',  // -2 : "/3/0/"
        0x00, 0x61, '1',              // 0 : "1"
        0x02, 0x07,                   // 2 : 7
    };
    auto codec = select_codec(transport::ContentFormat::SenmlCbor);
    auto records = codec->decode_write(data, device_types);
    ASSERT_TRUE(records);
    ASSERT_EQ(records.value().size(), 1u);
    EXPECT_EQ(records.value()[0].rid.value, 1);
    EXPECT_EQ(std::get<int64_t>(records.value()[0].value), 7);
}

}  // namespace
