// Ferriot - Observe/Notify Unit Tests

#include <gtest/gtest.h>

#include "lwm2m/observe.hpp"

namespace lwm2m {
namespace {

// --- FNV-1a hash (change detection for instance/object observations) ---

TEST(ObserveHashTest, StableForSameBytes) {
    std::vector<uint8_t> a{1, 2, 3, 4};
    std::vector<uint8_t> b{1, 2, 3, 4};
    EXPECT_EQ(fnv1a(a), fnv1a(b));
}

TEST(ObserveHashTest, ChangesWhenBytesChange) {
    std::vector<uint8_t> a{1, 2, 3, 4};
    std::vector<uint8_t> b{1, 2, 3, 5};
    EXPECT_NE(fnv1a(a), fnv1a(b));
}

TEST(ObserveHashTest, EmptyIsOffsetBasis) {
    EXPECT_EQ(fnv1a({}), 1469598103934665603ULL);
}

// --- ObserveKey identity (session, token) ---

TEST(ObserveKeyTest, EqualWhenSessionAndTokenMatch) {
    ObserveKey a{1, {0xAA, 0xBB}};
    ObserveKey b{1, {0xAA, 0xBB}};
    EXPECT_TRUE(a == b);
    EXPECT_EQ(ObserveKeyHash{}(a), ObserveKeyHash{}(b));
}

TEST(ObserveKeyTest, DiffersBySession) {
    ObserveKey a{1, {0xAA}};
    ObserveKey b{2, {0xAA}};
    EXPECT_FALSE(a == b);
}

TEST(ObserveKeyTest, DiffersByToken) {
    ObserveKey a{1, {0xAA}};
    ObserveKey b{1, {0xBB}};
    EXPECT_FALSE(a == b);
}

// --- ObjectPath equality (used to compare observed paths and ResourceValue) ---

TEST(ObjectPathEqualityTest, SamePathEqual) {
    auto a = ObjectPath::parse("/3/0/9");
    auto b = ObjectPath::parse("/3/0/9");
    ASSERT_TRUE(a && b);
    EXPECT_TRUE(*a == *b);
}

TEST(ObjectPathEqualityTest, DifferentDepthNotEqual) {
    auto obj = ObjectPath::parse("/3");
    auto inst = ObjectPath::parse("/3/0");
    auto res = ObjectPath::parse("/3/0/9");
    ASSERT_TRUE(obj && inst && res);
    EXPECT_FALSE(*obj == *inst);
    EXPECT_FALSE(*inst == *res);
}

TEST(ObjectPathEqualityTest, DifferentResourceNotEqual) {
    auto a = ObjectPath::parse("/3/0/9");
    auto b = ObjectPath::parse("/3/0/10");
    ASSERT_TRUE(a && b);
    EXPECT_FALSE(*a == *b);
}

// --- ResourceValue equality (change detection for resource observations) ---

TEST(ObserveResourceValueTest, IntegerCompare) {
    ResourceValue a = int64_t{42};
    ResourceValue b = int64_t{42};
    ResourceValue c = int64_t{43};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ObserveResourceValueTest, StringCompare) {
    ResourceValue a = std::string("Ferriot");
    ResourceValue b = std::string("Ferriot");
    ResourceValue c = std::string("other");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ObserveResourceValueTest, DifferentAlternativesNotEqual) {
    ResourceValue a = int64_t{1};
    ResourceValue b = std::string("1");
    EXPECT_FALSE(a == b);
}

}  // namespace
}  // namespace lwm2m
