// Ferriot - Objects Unit Tests

#include <gtest/gtest.h>

#include "lwm2m/objects/security.hpp"
#include "lwm2m/objects/server.hpp"
#include "lwm2m/objects/device.hpp"
#include "lwm2m/objects/location.hpp"
#include "lwm2m/objects/firmware_update.hpp"
#include "lwm2m/objects/connectivity.hpp"

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

// Location Object Tests
class LocationObjectTest : public ::testing::Test {
protected:
    LocationObject location;
};

TEST_F(LocationObjectTest, ObjectId) {
    EXPECT_EQ(location.id().value, 6);
    EXPECT_EQ(location.name(), "Location");
}

TEST_F(LocationObjectTest, SingleInstance) {
    auto instances = location.list_instances();
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_EQ(instances[0].value, 0);

    EXPECT_TRUE(location.has_instance(InstanceId{0}));
    EXPECT_FALSE(location.has_instance(InstanceId{1}));
}

TEST_F(LocationObjectTest, CannotCreateDeleteInstance) {
    auto create_result = location.create_instance(std::nullopt);
    EXPECT_TRUE(create_result.is_err());

    auto delete_result = location.delete_instance(InstanceId{0});
    EXPECT_TRUE(delete_result.is_err());
}

TEST_F(LocationObjectTest, InitiallyNoPosition) {
    EXPECT_FALSE(location.has_position());

    auto result = location.read_resource(InstanceId{0}, location_resource::Latitude, std::nullopt);
    EXPECT_TRUE(result.is_err());
}

TEST_F(LocationObjectTest, UpdatePosition) {
    GpsPosition pos;
    pos.latitude = 41.3851;
    pos.longitude = 2.1734;
    pos.altitude = 100.0;
    pos.speed = 5.5;
    pos.radius = 10.0;
    pos.timestamp = std::chrono::system_clock::now();

    location.update_position(pos);

    EXPECT_TRUE(location.has_position());
    EXPECT_DOUBLE_EQ(location.position().latitude, 41.3851);
    EXPECT_DOUBLE_EQ(location.position().longitude, 2.1734);
}

TEST_F(LocationObjectTest, ReadLatitude) {
    GpsPosition pos;
    pos.latitude = 41.3851;
    pos.longitude = 2.1734;
    location.update_position(pos);

    auto result = location.read_resource(InstanceId{0}, location_resource::Latitude, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_DOUBLE_EQ(std::get<double>(result.value()), 41.3851);
}

TEST_F(LocationObjectTest, ReadLongitude) {
    GpsPosition pos;
    pos.latitude = 41.3851;
    pos.longitude = 2.1734;
    location.update_position(pos);

    auto result = location.read_resource(InstanceId{0}, location_resource::Longitude, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_DOUBLE_EQ(std::get<double>(result.value()), 2.1734);
}

TEST_F(LocationObjectTest, ReadAltitude) {
    GpsPosition pos;
    pos.latitude = 41.3851;
    pos.longitude = 2.1734;
    pos.altitude = 250.5;
    location.update_position(pos);

    auto result = location.read_resource(InstanceId{0}, location_resource::Altitude, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_DOUBLE_EQ(std::get<double>(result.value()), 250.5);
}

TEST_F(LocationObjectTest, ReadSpeed) {
    GpsPosition pos;
    pos.latitude = 41.3851;
    pos.longitude = 2.1734;
    pos.speed = 25.0;
    location.update_position(pos);

    auto result = location.read_resource(InstanceId{0}, location_resource::Speed, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_DOUBLE_EQ(std::get<double>(result.value()), 25.0);
}

TEST_F(LocationObjectTest, ReadTimestamp) {
    auto now = std::chrono::system_clock::now();
    GpsPosition pos;
    pos.latitude = 41.3851;
    pos.longitude = 2.1734;
    pos.timestamp = now;
    location.update_position(pos);

    auto result = location.read_resource(InstanceId{0}, location_resource::Timestamp, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    auto timestamp = std::get<std::chrono::system_clock::time_point>(result.value());
    EXPECT_EQ(timestamp, now);
}

TEST_F(LocationObjectTest, ResourcesAreReadOnly) {
    GpsPosition pos;
    pos.latitude = 41.3851;
    pos.longitude = 2.1734;
    location.update_position(pos);

    auto result = location.write_resource(
        InstanceId{0}, location_resource::Latitude, std::nullopt, 50.0
    );
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(LocationObjectTest, NoExecutableResources) {
    auto result = location.execute_resource(InstanceId{0}, ResourceId{0}, "");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(LocationObjectTest, GpsProvider) {
    int call_count = 0;
    location.set_gps_provider([&]() -> std::optional<GpsPosition> {
        call_count++;
        GpsPosition pos;
        pos.latitude = 40.4168;
        pos.longitude = -3.7038;
        return pos;
    });

    // First read should trigger provider
    auto result = location.read_resource(InstanceId{0}, location_resource::Latitude, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_DOUBLE_EQ(std::get<double>(result.value()), 40.4168);
    EXPECT_GE(call_count, 1);
}

TEST_F(LocationObjectTest, InvalidPositionNotAccepted) {
    GpsPosition invalid_pos;
    invalid_pos.latitude = 200.0;  // Invalid: out of range
    invalid_pos.longitude = 0.0;

    location.update_position(invalid_pos);
    EXPECT_FALSE(location.has_position());
}

TEST_F(LocationObjectTest, GpsPositionValidity) {
    GpsPosition pos;

    // Valid position
    pos.latitude = 45.0;
    pos.longitude = -90.0;
    EXPECT_TRUE(pos.is_valid());

    // Invalid latitude
    pos.latitude = 91.0;
    EXPECT_FALSE(pos.is_valid());

    // Invalid longitude
    pos.latitude = 45.0;
    pos.longitude = 181.0;
    EXPECT_FALSE(pos.is_valid());
}

// Firmware Update Object Tests
class FirmwareUpdateObjectTest : public ::testing::Test {
protected:
    FirmwareUpdateObject firmware;
};

TEST_F(FirmwareUpdateObjectTest, ObjectId) {
    EXPECT_EQ(firmware.id().value, 5);
    EXPECT_EQ(firmware.name(), "Firmware Update");
}

TEST_F(FirmwareUpdateObjectTest, SingleInstance) {
    auto instances = firmware.list_instances();
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_EQ(instances[0].value, 0);

    EXPECT_TRUE(firmware.has_instance(InstanceId{0}));
    EXPECT_FALSE(firmware.has_instance(InstanceId{1}));
}

TEST_F(FirmwareUpdateObjectTest, CannotCreateDeleteInstance) {
    auto create_result = firmware.create_instance(std::nullopt);
    EXPECT_TRUE(create_result.is_err());

    auto delete_result = firmware.delete_instance(InstanceId{0});
    EXPECT_TRUE(delete_result.is_err());
}

TEST_F(FirmwareUpdateObjectTest, InitialState) {
    EXPECT_EQ(firmware.state(), FirmwareState::Idle);
    EXPECT_EQ(firmware.update_result(), FirmwareUpdateResult::Initial);
}

TEST_F(FirmwareUpdateObjectTest, ReadState) {
    auto result = firmware.read_resource(InstanceId{0}, firmware_resource::State, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 0);  // Idle
}

TEST_F(FirmwareUpdateObjectTest, ReadUpdateResult) {
    auto result = firmware.read_resource(InstanceId{0}, firmware_resource::UpdateResult, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 0);  // Initial
}

TEST_F(FirmwareUpdateObjectTest, ReadDeliveryMethod) {
    auto result = firmware.read_resource(InstanceId{0}, firmware_resource::DeliveryMethod, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 2);  // Both
}

TEST_F(FirmwareUpdateObjectTest, PackageIsWriteOnly) {
    auto result = firmware.read_resource(InstanceId{0}, firmware_resource::Package, std::nullopt);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(FirmwareUpdateObjectTest, PackageUriIsWriteOnly) {
    auto result = firmware.read_resource(InstanceId{0}, firmware_resource::PackageUri, std::nullopt);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(FirmwareUpdateObjectTest, PushPackage) {
    bool verify_called = false;
    FirmwareUpdateCallbacks callbacks;
    callbacks.verify_package = [&](const std::vector<uint8_t>& /* data */) -> Result<void> {
        verify_called = true;
        return Ok();
    };
    firmware.set_callbacks(callbacks);

    std::vector<uint8_t> package = {0x01, 0x02, 0x03, 0x04};
    auto result = firmware.write_resource(
        InstanceId{0}, firmware_resource::Package, std::nullopt, package
    );

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(verify_called);
    EXPECT_EQ(firmware.state(), FirmwareState::Downloaded);
}

TEST_F(FirmwareUpdateObjectTest, CannotPushPackageWhileDownloading) {
    firmware.set_state(FirmwareState::Downloading);

    std::vector<uint8_t> package = {0x01, 0x02, 0x03, 0x04};
    auto result = firmware.write_resource(
        InstanceId{0}, firmware_resource::Package, std::nullopt, package
    );

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(FirmwareUpdateObjectTest, WritePackageUri) {
    bool download_started = false;
    FirmwareUpdateCallbacks callbacks;
    callbacks.start_download = [&](const std::string& /* uri */) -> Result<void> {
        download_started = true;
        return Ok();
    };
    firmware.set_callbacks(callbacks);

    auto result = firmware.write_resource(
        InstanceId{0}, firmware_resource::PackageUri, std::nullopt,
        std::string{"coap://example.com/firmware.bin"}
    );

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(download_started);
    EXPECT_EQ(firmware.state(), FirmwareState::Downloading);
}

TEST_F(FirmwareUpdateObjectTest, ExecuteUpdateNotInDownloadedState) {
    EXPECT_EQ(firmware.state(), FirmwareState::Idle);

    auto result = firmware.execute_resource(InstanceId{0}, firmware_resource::Update, "");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(FirmwareUpdateObjectTest, ExecuteUpdate) {
    bool update_applied = false;
    FirmwareUpdateCallbacks callbacks;
    callbacks.verify_package = [](const std::vector<uint8_t>& /* data */) -> Result<void> {
        return Ok();
    };
    callbacks.apply_update = [&]() -> Result<void> {
        update_applied = true;
        return Ok();
    };
    firmware.set_callbacks(callbacks);

    // Push package to get to Downloaded state
    std::vector<uint8_t> package = {0x01, 0x02, 0x03};
    (void)firmware.write_resource(InstanceId{0}, firmware_resource::Package, std::nullopt, package);
    EXPECT_EQ(firmware.state(), FirmwareState::Downloaded);

    // Execute update
    auto result = firmware.execute_resource(InstanceId{0}, firmware_resource::Update, "");
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(update_applied);
    EXPECT_EQ(firmware.state(), FirmwareState::Updating);
}

TEST_F(FirmwareUpdateObjectTest, DownloadComplete) {
    firmware.set_state(FirmwareState::Downloading);

    firmware.download_complete(true);
    EXPECT_EQ(firmware.state(), FirmwareState::Downloaded);

    firmware.set_state(FirmwareState::Downloading);
    firmware.download_complete(false);
    EXPECT_EQ(firmware.state(), FirmwareState::Idle);
    EXPECT_EQ(firmware.update_result(), FirmwareUpdateResult::ConnectionLostDuringDownload);
}

TEST_F(FirmwareUpdateObjectTest, UpdateComplete) {
    firmware.set_state(FirmwareState::Updating);

    firmware.update_complete(FirmwareUpdateResult::Success);
    EXPECT_EQ(firmware.state(), FirmwareState::Idle);
    EXPECT_EQ(firmware.update_result(), FirmwareUpdateResult::Success);
}

TEST_F(FirmwareUpdateObjectTest, SetPackageInfo) {
    firmware.set_package_info("test-firmware", "2.0.0");

    auto name_result = firmware.read_resource(InstanceId{0}, firmware_resource::PkgName, std::nullopt);
    ASSERT_TRUE(name_result.is_ok());
    EXPECT_EQ(std::get<std::string>(name_result.value()), "test-firmware");

    auto version_result = firmware.read_resource(InstanceId{0}, firmware_resource::PkgVersion, std::nullopt);
    ASSERT_TRUE(version_result.is_ok());
    EXPECT_EQ(std::get<std::string>(version_result.value()), "2.0.0");
}

// Connectivity Monitoring Object Tests
class ConnectivityObjectTest : public ::testing::Test {
protected:
    static ConnectivityConfig make_config() {
        ConnectivityConfig cfg;
        cfg.network_bearer = NetworkBearer::LTE;
        cfg.available_bearers = {NetworkBearer::LTE, NetworkBearer::WiFi};
        cfg.radio_signal_strength = -65;
        cfg.link_quality = 85;
        cfg.ip_addresses = {"192.168.1.100", "10.0.0.50"};
        cfg.router_ip_addresses = {"192.168.1.1"};
        cfg.link_utilization = 25;
        cfg.apn_list = {"internet", "iot.provider.com"};
        cfg.cell_id = 12345678;
        cfg.smcc = 214;  // Spain
        cfg.smnc = 7;    // Movistar
        return cfg;
    }
    ConnectivityConfig config = make_config();
    ConnectivityObject connectivity{config};
};

TEST_F(ConnectivityObjectTest, ObjectId) {
    EXPECT_EQ(connectivity.id().value, 4);
    EXPECT_EQ(connectivity.name(), "Connectivity Monitoring");
}

TEST_F(ConnectivityObjectTest, SingleInstance) {
    auto instances = connectivity.list_instances();
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_EQ(instances[0].value, 0);

    EXPECT_TRUE(connectivity.has_instance(InstanceId{0}));
    EXPECT_FALSE(connectivity.has_instance(InstanceId{1}));
}

TEST_F(ConnectivityObjectTest, CannotCreateDeleteInstance) {
    auto create_result = connectivity.create_instance(std::nullopt);
    EXPECT_TRUE(create_result.is_err());

    auto delete_result = connectivity.delete_instance(InstanceId{0});
    EXPECT_TRUE(delete_result.is_err());
}

TEST_F(ConnectivityObjectTest, ReadNetworkBearer) {
    auto result = connectivity.read_resource(InstanceId{0}, connectivity_resource::NetworkBearer, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 5);  // LTE
}

TEST_F(ConnectivityObjectTest, ReadAvailableBearers) {
    // First bearer
    auto result0 = connectivity.read_resource(
        InstanceId{0}, connectivity_resource::AvailableNetworkBearer, ResourceInstanceId{0}
    );
    ASSERT_TRUE(result0.is_ok());
    EXPECT_EQ(std::get<int64_t>(result0.value()), 5);  // LTE

    // Second bearer
    auto result1 = connectivity.read_resource(
        InstanceId{0}, connectivity_resource::AvailableNetworkBearer, ResourceInstanceId{1}
    );
    ASSERT_TRUE(result1.is_ok());
    EXPECT_EQ(std::get<int64_t>(result1.value()), 21);  // WiFi
}

TEST_F(ConnectivityObjectTest, ReadSignalStrength) {
    auto result = connectivity.read_resource(InstanceId{0}, connectivity_resource::RadioSignalStrength, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), -65);
}

TEST_F(ConnectivityObjectTest, ReadLinkQuality) {
    auto result = connectivity.read_resource(InstanceId{0}, connectivity_resource::LinkQuality, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 85);
}

TEST_F(ConnectivityObjectTest, ReadIpAddresses) {
    auto result = connectivity.read_resource(
        InstanceId{0}, connectivity_resource::IpAddresses, ResourceInstanceId{0}
    );
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<std::string>(result.value()), "192.168.1.100");

    auto result1 = connectivity.read_resource(
        InstanceId{0}, connectivity_resource::IpAddresses, ResourceInstanceId{1}
    );
    ASSERT_TRUE(result1.is_ok());
    EXPECT_EQ(std::get<std::string>(result1.value()), "10.0.0.50");
}

TEST_F(ConnectivityObjectTest, ReadCellularInfo) {
    auto cell_result = connectivity.read_resource(InstanceId{0}, connectivity_resource::CellId, std::nullopt);
    ASSERT_TRUE(cell_result.is_ok());
    EXPECT_EQ(std::get<int64_t>(cell_result.value()), 12345678);

    auto mcc_result = connectivity.read_resource(InstanceId{0}, connectivity_resource::Smcc, std::nullopt);
    ASSERT_TRUE(mcc_result.is_ok());
    EXPECT_EQ(std::get<int64_t>(mcc_result.value()), 214);

    auto mnc_result = connectivity.read_resource(InstanceId{0}, connectivity_resource::Smnc, std::nullopt);
    ASSERT_TRUE(mnc_result.is_ok());
    EXPECT_EQ(std::get<int64_t>(mnc_result.value()), 7);
}

TEST_F(ConnectivityObjectTest, ReadApn) {
    auto result = connectivity.read_resource(
        InstanceId{0}, connectivity_resource::Apn, ResourceInstanceId{0}
    );
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<std::string>(result.value()), "internet");
}

TEST_F(ConnectivityObjectTest, ResourcesAreReadOnly) {
    auto result = connectivity.write_resource(
        InstanceId{0}, connectivity_resource::NetworkBearer, std::nullopt, static_cast<int64_t>(21)
    );
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(ConnectivityObjectTest, NoExecutableResources) {
    auto result = connectivity.execute_resource(InstanceId{0}, ResourceId{0}, "");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::MethodNotAllowed);
}

TEST_F(ConnectivityObjectTest, SetNetworkBearer) {
    connectivity.set_network_bearer(NetworkBearer::WiFi);

    auto result = connectivity.read_resource(InstanceId{0}, connectivity_resource::NetworkBearer, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 21);  // WiFi
}

TEST_F(ConnectivityObjectTest, SetSignalStrength) {
    connectivity.set_signal_strength(-95);

    auto result = connectivity.read_resource(InstanceId{0}, connectivity_resource::RadioSignalStrength, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), -95);
}

TEST_F(ConnectivityObjectTest, SetLinkQualityCapped) {
    connectivity.set_link_quality(150);  // Should be capped to 100

    auto result = connectivity.read_resource(InstanceId{0}, connectivity_resource::LinkQuality, std::nullopt);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<int64_t>(result.value()), 100);
}

TEST_F(ConnectivityObjectTest, SetCellularInfo) {
    connectivity.set_cellular_info(98765432, 310, 260);  // US T-Mobile

    auto cell_result = connectivity.read_resource(InstanceId{0}, connectivity_resource::CellId, std::nullopt);
    ASSERT_TRUE(cell_result.is_ok());
    EXPECT_EQ(std::get<int64_t>(cell_result.value()), 98765432);

    auto mcc_result = connectivity.read_resource(InstanceId{0}, connectivity_resource::Smcc, std::nullopt);
    ASSERT_TRUE(mcc_result.is_ok());
    EXPECT_EQ(std::get<int64_t>(mcc_result.value()), 310);

    auto mnc_result = connectivity.read_resource(InstanceId{0}, connectivity_resource::Smnc, std::nullopt);
    ASSERT_TRUE(mnc_result.is_ok());
    EXPECT_EQ(std::get<int64_t>(mnc_result.value()), 260);
}

TEST_F(ConnectivityObjectTest, SetIpAddresses) {
    connectivity.set_ip_addresses({"10.20.30.40"});

    auto result = connectivity.read_resource(
        InstanceId{0}, connectivity_resource::IpAddresses, ResourceInstanceId{0}
    );
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::get<std::string>(result.value()), "10.20.30.40");
}

}  // namespace
}  // namespace lwm2m::objects
