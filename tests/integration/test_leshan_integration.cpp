// Ferriot - Leshan Integration Tests
// These tests require a running Leshan server at localhost:8080

#include "leshan_api.hpp"

#include <lwm2m/client.hpp>
#include <lwm2m/objects/security.hpp>
#include <lwm2m/objects/server.hpp>
#include <lwm2m/objects/device.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace lwm2m::test {

// Test fixture for Leshan integration tests
class LeshanIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        leshan_ = std::make_unique<LeshanApi>("http://localhost:8080");

        // Skip tests if Leshan is not available
        if (!leshan_->is_server_available()) {
            GTEST_SKIP() << "Leshan server not available at localhost:8080";
        }
    }

    void TearDown() override {
        // Clean up any test clients that might be left over
        if (leshan_ && !test_endpoint_.empty()) {
            if (leshan_->client_exists(test_endpoint_)) {
                (void)leshan_->delete_client(test_endpoint_);
            }
        }
    }

    // Create a test client with unique endpoint name
    std::unique_ptr<Client> create_test_client(const std::string& endpoint_suffix = "") {
        test_endpoint_ = "test-cpp-client";
        if (!endpoint_suffix.empty()) {
            test_endpoint_ += "-" + endpoint_suffix;
        }
        // Add timestamp for uniqueness
        test_endpoint_ += "-" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count() % 100000
        );

        ClientConfig config;
        config.endpoint_name = test_endpoint_;
        config.lifetime = "60";  // Short lifetime for tests
        config.binding_mode = "U";

        auto client = std::make_unique<Client>(config);

        // Configure security (NoSec for local testing)
        objects::SecurityInstance sec_instance;
        sec_instance.server_uri = "coap://127.0.0.1:5683";
        sec_instance.bootstrap_server = false;
        sec_instance.security_mode = objects::SecurityModeValue::NoSec;
        sec_instance.short_server_id = 1;
        client->security().add_server(sec_instance);

        // Configure server
        objects::ServerInstance srv_instance;
        srv_instance.short_server_id = 1;
        srv_instance.lifetime = 60;
        srv_instance.binding = "U";
        client->server().add_server(srv_instance);

        // Configure device
        objects::DeviceConfig dev_config;
        dev_config.manufacturer = "LWM2M-CPP-Test";
        dev_config.model_number = "LWM2M-CPP-Test";
        dev_config.serial_number = test_endpoint_;
        dev_config.firmware_version = "1.0.0";
        client->device() = objects::DeviceObject(dev_config);

        return client;
    }

    std::unique_ptr<LeshanApi> leshan_;
    std::string test_endpoint_;
};

// ============================================================================
// Leshan API Tests
// ============================================================================

TEST_F(LeshanIntegrationTest, ServerIsAvailable) {
    EXPECT_TRUE(leshan_->is_server_available());
}

TEST_F(LeshanIntegrationTest, GetClientsReturnsArray) {
    auto clients = leshan_->get_clients();
    // Should return at least an empty array without error
    EXPECT_GE(clients.size(), 0u);
}

TEST_F(LeshanIntegrationTest, NonExistentClientReturnsEmpty) {
    auto client = leshan_->get_client("non-existent-endpoint-12345");
    EXPECT_FALSE(client.has_value());
}

// ============================================================================
// Client Registration Tests
// ============================================================================

TEST_F(LeshanIntegrationTest, ClientRegistration) {
    auto client = create_test_client("reg");

    // Track state changes
    std::atomic<bool> registered{false};
    std::atomic<bool> state_changed{false};
    ClientState last_state = ClientState::Idle;

    ClientCallbacks callbacks;
    callbacks.on_state_change = [&](ClientState old_state, ClientState new_state) {
        (void)old_state;
        last_state = new_state;
        state_changed = true;
    };
    callbacks.on_registered = [&](uint16_t ssid, const Registration& reg) {
        (void)ssid;
        (void)reg;
        registered = true;
    };
    client->set_callbacks(callbacks);

    auto start_result = client->start();
    ASSERT_TRUE(start_result) << "Failed to start client: " << start_result.error().message();

    auto reg_result = client->register_with_server(1);
    ASSERT_TRUE(reg_result) << "Failed to register: " << reg_result.error().message();

    // Wait for registration to appear in Leshan
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}))
        << "Client did not appear in Leshan: " << leshan_->last_error();

    // Verify client info
    auto leshan_client = leshan_->get_client(test_endpoint_);
    ASSERT_TRUE(leshan_client.has_value());
    EXPECT_EQ(leshan_client->endpoint, test_endpoint_);
    EXPECT_EQ(leshan_client->binding_mode, "U");
    EXPECT_EQ(leshan_client->lifetime, 60u);

    client->stop();
}

TEST_F(LeshanIntegrationTest, ClientDeregistration) {
    auto client = create_test_client("dereg");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Deregister
    auto dereg_result = client->deregister(1);
    EXPECT_TRUE(dereg_result) << "Deregister failed: " << dereg_result.error().message();

    // Wait for client to disappear from Leshan
    EXPECT_TRUE(leshan_->wait_for_client_gone(test_endpoint_, std::chrono::seconds{10}))
        << "Client still visible after deregister";

    client->stop();
}

TEST_F(LeshanIntegrationTest, ClientUpdateRegistration) {
    auto client = create_test_client("update");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Get initial lastUpdate timestamp
    auto client_before = leshan_->get_client(test_endpoint_);
    ASSERT_TRUE(client_before.has_value());
    auto last_update_before = client_before->last_update;

    // Wait a bit and update registration
    std::this_thread::sleep_for(std::chrono::seconds{2});
    auto update_result = client->update_registration(1);
    EXPECT_TRUE(update_result) << "Update failed: " << update_result.error().message();

    // Give Leshan time to process
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // Verify lastUpdate changed
    auto client_after = leshan_->get_client(test_endpoint_);
    ASSERT_TRUE(client_after.has_value());
    EXPECT_GT(client_after->last_update, last_update_before);

    client->stop();
}

// ============================================================================
// Resource Read Tests (via Leshan)
// ============================================================================

TEST_F(LeshanIntegrationTest, ReadDeviceManufacturer) {
    auto client = create_test_client("read");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Give client time to stabilize polling loop before read request
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // Read manufacturer via Leshan (Object 3, Instance 0, Resource 0)
    auto value = leshan_->read_resource(test_endpoint_, 3, 0, 0);
    ASSERT_TRUE(value.has_value()) << "Read failed: " << leshan_->last_error();

    EXPECT_EQ(value->type, LeshanResourceValue::Type::String);
    EXPECT_EQ(value->string_value, "LWM2M-CPP-Test");

    client->stop();
}

TEST_F(LeshanIntegrationTest, ReadDeviceModelNumber) {
    auto client = create_test_client("model");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Give client time to stabilize polling loop before read request
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // Read model number via Leshan (Object 3, Instance 0, Resource 1)
    auto value = leshan_->read_resource(test_endpoint_, 3, 0, 1);
    ASSERT_TRUE(value.has_value()) << "Read failed: " << leshan_->last_error();

    EXPECT_EQ(value->type, LeshanResourceValue::Type::String);
    EXPECT_EQ(value->string_value, "LWM2M-CPP-Test");

    client->stop();
}

TEST_F(LeshanIntegrationTest, ReadServerLifetime) {
    auto client = create_test_client("lifetime");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Give client time to stabilize polling loop before read request
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // Read lifetime via Leshan (Object 1, Instance 0, Resource 1)
    auto value = leshan_->read_resource(test_endpoint_, 1, 0, 1);
    ASSERT_TRUE(value.has_value()) << "Read failed: " << leshan_->last_error();

    EXPECT_EQ(value->type, LeshanResourceValue::Type::Integer);
    EXPECT_EQ(value->int_value, 60);

    client->stop();
}

// ============================================================================
// Resource Write Tests (via Leshan)
// ============================================================================

TEST_F(LeshanIntegrationTest, WriteServerLifetime) {
    auto client = create_test_client("write");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Give client time to stabilize polling loop before write request
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    // Write new lifetime via Leshan (Object 1, Instance 0, Resource 1)
    LeshanResourceValue new_lifetime;
    new_lifetime.type = LeshanResourceValue::Type::Integer;
    new_lifetime.int_value = 120;

    bool write_success = leshan_->write_resource(test_endpoint_, 1, 0, 1, new_lifetime);
    ASSERT_TRUE(write_success) << "Write failed: " << leshan_->last_error();

    // Read back and verify
    auto read_value = leshan_->read_resource(test_endpoint_, 1, 0, 1);
    ASSERT_TRUE(read_value.has_value());
    EXPECT_EQ(read_value->int_value, 120);

    client->stop();
}

// ============================================================================
// Execute Tests (via Leshan)
// ============================================================================

TEST_F(LeshanIntegrationTest, ExecuteReboot) {
    auto client = create_test_client("exec");

    // Track if reboot was called
    std::atomic<bool> reboot_called{false};
    client->device().set_reboot_callback([&]() {
        reboot_called = true;
    });

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Execute reboot via Leshan (Object 3, Instance 0, Resource 4)
    bool exec_success = leshan_->execute_resource(test_endpoint_, 3, 0, 4);
    EXPECT_TRUE(exec_success) << "Execute failed: " << leshan_->last_error();

    // Give time for callback to be invoked
    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    EXPECT_TRUE(reboot_called) << "Reboot callback was not invoked";

    client->stop();
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(LeshanIntegrationTest, ReadNonExistentResource) {
    auto client = create_test_client("error");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Try to read non-existent resource (Object 3, Instance 0, Resource 999)
    auto value = leshan_->read_resource(test_endpoint_, 3, 0, 999);
    EXPECT_FALSE(value.has_value());

    client->stop();
}

TEST_F(LeshanIntegrationTest, WriteReadOnlyResource) {
    auto client = create_test_client("readonly");

    ASSERT_TRUE(client->start());
    ASSERT_TRUE(client->register_with_server(1));
    ASSERT_TRUE(leshan_->wait_for_client(test_endpoint_, std::chrono::seconds{10}));

    // Try to write to manufacturer (read-only resource)
    LeshanResourceValue new_value;
    new_value.type = LeshanResourceValue::Type::String;
    new_value.string_value = "Hacked";

    bool write_success = leshan_->write_resource(test_endpoint_, 3, 0, 0, new_value);
    EXPECT_FALSE(write_success) << "Write to read-only resource should fail";

    client->stop();
}

} // namespace lwm2m::test
