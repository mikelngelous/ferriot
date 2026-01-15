// Ferriot - Types Unit Tests

#include <gtest/gtest.h>

#include "lwm2m/types.hpp"

namespace lwm2m {
namespace {

TEST(StrongTypeTest, Construction) {
    ObjectId oid{42};
    EXPECT_EQ(oid.value, 42);

    InstanceId iid{0};
    EXPECT_EQ(iid.value, 0);
}

TEST(StrongTypeTest, DefaultConstruction) {
    ObjectId oid;
    EXPECT_EQ(oid.value, 0);
}

TEST(StrongTypeTest, Comparison) {
    ObjectId a{1};
    ObjectId b{1};
    ObjectId c{2};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_LT(a, c);
}

TEST(StrongTypeTest, TypeSafety) {
    // These should not compile (type safety):
    // ObjectId oid{42};
    // InstanceId iid = oid;  // Error: different types
}

TEST(ObjectPathTest, ToStringObject) {
    auto path = ObjectPath::object(ObjectId{3});
    EXPECT_EQ(path.to_string(), "/3");
}

TEST(ObjectPathTest, ToStringInstance) {
    auto path = ObjectPath::instance(ObjectId{3}, InstanceId{0});
    EXPECT_EQ(path.to_string(), "/3/0");
}

TEST(ObjectPathTest, ToStringResource) {
    auto path = ObjectPath::resource(ObjectId{3}, InstanceId{0}, ResourceId{1});
    EXPECT_EQ(path.to_string(), "/3/0/1");
}

TEST(ObjectPathTest, ParseObject) {
    auto result = ObjectPath::parse("/3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->object_id.value, 3);
    EXPECT_FALSE(result->instance_id.has_value());
    EXPECT_TRUE(result->is_object());
}

TEST(ObjectPathTest, ParseInstance) {
    auto result = ObjectPath::parse("/3/0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->object_id.value, 3);
    EXPECT_EQ(result->instance_id->value, 0);
    EXPECT_FALSE(result->resource_id.has_value());
    EXPECT_TRUE(result->is_instance());
}

TEST(ObjectPathTest, ParseResource) {
    auto result = ObjectPath::parse("/3/0/1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->object_id.value, 3);
    EXPECT_EQ(result->instance_id->value, 0);
    EXPECT_EQ(result->resource_id->value, 1);
    EXPECT_TRUE(result->is_resource());
}

TEST(ObjectPathTest, ParseResourceInstance) {
    auto result = ObjectPath::parse("/3/0/11/0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->object_id.value, 3);
    EXPECT_EQ(result->instance_id->value, 0);
    EXPECT_EQ(result->resource_id->value, 11);
    EXPECT_EQ(result->resource_instance_id->value, 0);
    EXPECT_TRUE(result->is_resource_instance());
}

TEST(ObjectPathTest, ParseInvalid) {
    EXPECT_FALSE(ObjectPath::parse("").has_value());
    EXPECT_FALSE(ObjectPath::parse("3").has_value());       // Missing leading /
    EXPECT_FALSE(ObjectPath::parse("//3").has_value());     // Empty segment
    EXPECT_FALSE(ObjectPath::parse("/abc").has_value());    // Non-numeric
    EXPECT_FALSE(ObjectPath::parse("/3/").has_value());     // Trailing slash with empty
}

TEST(ResourceAccessTest, Bitwise) {
    auto rw = ResourceAccess::Read | ResourceAccess::Write;
    EXPECT_TRUE(has_access(rw, ResourceAccess::Read));
    EXPECT_TRUE(has_access(rw, ResourceAccess::Write));
    EXPECT_FALSE(has_access(rw, ResourceAccess::Execute));

    EXPECT_TRUE(has_access(ResourceAccess::ReadWrite, ResourceAccess::Read));
    EXPECT_TRUE(has_access(ResourceAccess::ReadWrite, ResourceAccess::Write));
}

TEST(StandardObjectIdsTest, Values) {
    EXPECT_EQ(object_id::Security.value, 0);
    EXPECT_EQ(object_id::Server.value, 1);
    EXPECT_EQ(object_id::AccessControl.value, 2);
    EXPECT_EQ(object_id::Device.value, 3);
    EXPECT_EQ(object_id::ConnectivityMonitoring.value, 4);
    EXPECT_EQ(object_id::FirmwareUpdate.value, 5);
    EXPECT_EQ(object_id::Location.value, 6);
}

}  // namespace
}  // namespace lwm2m
