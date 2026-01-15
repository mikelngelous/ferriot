// Ferriot - Objects Unit Tests

#include <gtest/gtest.h>

#include "lwm2m/objects/security.hpp"
#include "lwm2m/objects/server.hpp"
#include "lwm2m/objects/device.hpp"

namespace lwm2m::objects {
namespace {

// Security Object Tests
class SecurityObjectTest : public ::testing::Test {
protected:
    SecurityObject security;
};

TEST_F(SecurityObjectTest, ObjectId) {
    EXPECT_EQ(security.id().value, 0);
    EXPECT_EQ(security.name(), "Security");
}

TEST_F(SecurityObjectTest, InitiallyEmpty) {
    EXPECT_TRUE(security.list_instances().empty());
}

TEST_F(SecurityObjectTest, CreateInstance) {
    auto result = security.create_instance(std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().value, 0);

    auto instances = security.list_instances();
    EXPECT_EQ(instances.size(), 1u);
}

TEST_F(SecurityObjectTest, CreateInstanceWithId) {
    auto result = security.create_instance(InstanceId{5});
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().value, 5);
}

TEST_F(SecurityObjectTest, CreateDuplicateFails) {
    auto result1 = security.create_instance(InstanceId{0});
    ASSERT_TRUE(result1.is_ok());

    auto result2 = security.create_instance(InstanceId{0});
    EXPECT_TRUE(result2.is_err());
}

TEST_F(SecurityObjectTest, DeleteInstance) {
    (void)security.create_instance(std::nullopt);
    EXPECT_TRUE(security.has_instance(InstanceId{0}));

    auto result = security.delete_instance(InstanceId{0});
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(security.has_instance(InstanceId{0}));
}

TEST_F(SecurityObjectTest, AddServer) {
    SecurityInstance config;
    config.server_uri = "coaps://localhost:5684";
    config.bootstrap_server = false;
    config.security_mode = SecurityModeValue::NoSec;
    config.short_server_id = 1;

    auto result = security.add_server(config);
    ASSERT_TRUE(result.is_ok());

    auto* inst = security.get_instance(result.value());
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->server_uri, "coaps://localhost:5684");
    EXPECT_EQ(inst->short_server_id, 1);
}

TEST_F(SecurityObjectTest, ReadWriteResources) {
    (void)security.create_instance(std::nullopt);

    // Write server URI
    auto write_result = security.write_resource(
        InstanceId{0}, ResourceId{0}, std::nullopt,
        std::string{"coap://example.com:5683"}
    );
    EXPECT_TRUE(write_result.is_ok());

    // Read server URI
    auto read_result = security.read_resource(InstanceId{0}, ResourceId{0}, std::nullopt);
    ASSERT_TRUE(read_result.is_ok());
    EXPECT_EQ(std::get<std::string>(read_result.value()), "coap://example.com:5683");
}

// Server Object Tests
class ServerObjectTest : public ::testing::Test {
protected:
    ServerObject server;
};

TEST_F(ServerObjectTest, ObjectId) {
    EXPECT_EQ(server.id().value, 1);
    EXPECT_EQ(server.name(), "Server");
}

TEST_F(ServerObjectTest, AddServer) {
    ServerInstance config;
    config.short_server_id = 1;
    config.lifetime = 3600;
    config.binding = "U";

    auto result = server.add_server(config);
    ASSERT_TRUE(result.is_ok());

    auto* inst = server.get_instance(result.value());
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->lifetime, 3600u);
}

TEST_F(ServerObjectTest, FindByShortServerId) {
    ServerInstance config;
    config.short_server_id = 42;
    (void)server.add_server(config);

    auto found = server.find_by_short_server_id(42);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->short_server_id, 42);

    auto not_found = server.find_by_short_server_id(999);
    EXPECT_FALSE(not_found.has_value());
}

TEST_F(ServerObjectTest, ExecuteRegistrationUpdate) {
    (void)server.create_instance(std::nullopt);

    bool callback_called = false;
    server.set_registration_update_callback([&](uint16_t) {
        callback_called = true;
    });

    auto result = server.execute_resource(
        InstanceId{0}, server_resource::RegistrationUpdateTrigger, ""
    );
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(callback_called);
}

// Device Object Tests
class DeviceObjectTest : public ::testing::Test {
protected:
    static DeviceConfig make_config() {
        DeviceConfig cfg;
        cfg.manufacturer = "TestCorp";
        cfg.model_number = "TEST-001";
        cfg.serial_number = "SN12345";
        cfg.firmware_version = "1.0.0";
        return cfg;
    }
    DeviceConfig config = make_config();
    DeviceObject device{config};
};

TEST_F(DeviceObjectTest, ObjectId) {
    EXPECT_EQ(device.id().value, 3);
    EXPECT_EQ(device.name(), "Device");
}

TEST_F(DeviceObjectTest, SingleInstance) {
    auto instances = device.list_instances();
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_EQ(instances[0].value, 0);

    EXPECT_TRUE(device.has_instance(InstanceId{0}));
    EXPECT_FALSE(device.has_instance(InstanceId{1}));
}

TEST_F(DeviceObjectTest, CannotCreateDeleteInstance) {
    auto create_result = device.create_instance(std::nullopt);
    EXPECT_TRUE(create_result.is_err());

    auto delete_result = device.delete_instance(InstanceId{0});
    EXPECT_TRUE(delete_result.is_err());
}

TEST_F(DeviceObjectTest, ReadManufacturer) {
    auto result = device.read_resource(InstanceId{0}, device_resource::Manufacturer, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<std::string>(result.value()), "TestCorp");
}

TEST_F(DeviceObjectTest, ReadModelNumber) {
    auto result = device.read_resource(InstanceId{0}, device_resource::ModelNumber, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<std::string>(result.value()), "TEST-001");
}

TEST_F(DeviceObjectTest, ReadCurrentTime) {
    auto before = std::chrono::system_clock::now();
    auto result = device.read_resource(InstanceId{0}, device_resource::CurrentTime, std::nullopt);
    auto after = std::chrono::system_clock::now();

    ASSERT_TRUE(result.is_ok());
    auto time = std::get<std::chrono::system_clock::time_point>(result.value());

    EXPECT_GE(time, before);
    EXPECT_LE(time, after);
}

TEST_F(DeviceObjectTest, SetBatteryLevel) {
    device.set_battery_level(75);

    auto result = device.read_resource(InstanceId{0}, device_resource::BatteryLevel, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 75);
}

TEST_F(DeviceObjectTest, ExecuteReboot) {
    bool reboot_called = false;
    device.set_reboot_callback([&]() { reboot_called = true; });

    auto result = device.execute_resource(InstanceId{0}, device_resource::Reboot, "");
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(reboot_called);
}

TEST_F(DeviceObjectTest, ErrorCodes) {
    // Initial state
    auto result = device.read_resource(InstanceId{0}, device_resource::ErrorCode, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 0);  // NoError

    // Add error
    device.add_error_code(DeviceErrorCode::LowBatteryPower);
    result = device.read_resource(InstanceId{0}, device_resource::ErrorCode, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 1);  // LowBatteryPower

    // Clear errors
    device.clear_error_codes();
    result = device.read_resource(InstanceId{0}, device_resource::ErrorCode, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 0);  // NoError
}

}  // namespace
}  // namespace lwm2m::objects
