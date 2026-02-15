//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "service/common/bluetooth/util/address_helper.h"
#include "service/gatt_server.h"
#include "service/hal/fake_bluetooth_gatt_interface.h"
#include "service/hal/gatt_helpers.h"

using ::testing::_;
using ::testing::Return;

namespace bluetooth {
namespace {

class MockGattHandler
    : public hal::FakeBluetoothGattInterface::TestServerHandler {
 public:
  MockGattHandler() = default;
  ~MockGattHandler() override = default;

  MOCK_METHOD1(RegisterServer, bt_status_t(bt_uuid_t*));
  MOCK_METHOD1(UnregisterServer, bt_status_t(int));
  MOCK_METHOD3(AddService, bt_status_t(int, btgatt_srvc_id_t*, int));
  MOCK_METHOD5(AddCharacteristic, bt_status_t(int, int, bt_uuid_t*, int, int));
  MOCK_METHOD4(AddDescriptor, bt_status_t(int, int, bt_uuid_t*, int));
  MOCK_METHOD3(StartService, bt_status_t(int, int, int));
  MOCK_METHOD2(DeleteService, bt_status_t(int, int));
  MOCK_METHOD6(SendIndication, bt_status_t(int, int, int, int, int, char*));
  MOCK_METHOD4(SendResponse, bt_status_t(int, int, int, btgatt_response_t*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGattHandler);
};

class TestDelegate : public GattServer::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  struct RequestData {
    RequestData() : id(-1), offset(-1), is_long(false), is_prep(false),
                    need_rsp(false), is_exec(false), count(0) {}
    ~RequestData() = default;

    std::string device_address;
    int id;
    int offset;
    bool is_long;
    bool is_prep;
    bool need_rsp;
    bool is_exec;
    GattIdentifier gatt_id;
    int count;
    std::vector<uint8_t> write_value;
  };

  void OnCharacteristicReadRequest(
      GattServer* gatt_server,
      const std::string& device_address,
      int request_id, int offset, bool is_long,
      const bluetooth::GattIdentifier& characteristic_id) override {
    ASSERT_TRUE(gatt_server);
    char_read_req_.device_address = device_address;
    char_read_req_.id = request_id;
    char_read_req_.offset = offset;
    char_read_req_.is_long = is_long;
    char_read_req_.gatt_id = characteristic_id;
    char_read_req_.count++;
  }

  void OnDescriptorReadRequest(
      GattServer* gatt_server,
      const std::string& device_address,
      int request_id, int offset, bool is_long,
      const bluetooth::GattIdentifier& descriptor_id) override {
    ASSERT_TRUE(gatt_server);
    desc_read_req_.device_address = device_address;
    desc_read_req_.id = request_id;
    desc_read_req_.offset = offset;
    desc_read_req_.is_long = is_long;
    desc_read_req_.gatt_id = descriptor_id;
    desc_read_req_.count++;
  }

  void OnCharacteristicWriteRequest(
      GattServer* gatt_server,
      const std::string& device_address,
      int request_id, int offset, bool is_prepare_write, bool need_response,
      const std::vector<uint8_t>& value,
      const bluetooth::GattIdentifier& characteristic_id) override {
    ASSERT_TRUE(gatt_server);
    char_write_req_.device_address = device_address;
    char_write_req_.id = request_id;
    char_write_req_.offset = offset;
    char_write_req_.is_prep = is_prepare_write;
    char_write_req_.need_rsp = need_response;
    char_write_req_.gatt_id = characteristic_id;
    char_write_req_.count++;
    char_write_req_.write_value = value;
  }

  void OnDescriptorWriteRequest(
      GattServer* gatt_server,
      const std::string& device_address,
      int request_id, int offset, bool is_prepare_write, bool need_response,
      const std::vector<uint8_t>& value,
      const bluetooth::GattIdentifier& descriptor_id) override {
    ASSERT_TRUE(gatt_server);
    desc_write_req_.device_address = device_address;
    desc_write_req_.id = request_id;
    desc_write_req_.offset = offset;
    desc_write_req_.is_prep = is_prepare_write;
    desc_write_req_.need_rsp = need_response;
    desc_write_req_.gatt_id = descriptor_id;
    desc_write_req_.count++;
    desc_write_req_.write_value = value;
  }

  void OnExecuteWriteRequest(
      GattServer* gatt_server,
      const std::string& device_address,
      int request_id, bool is_execute) override {
    ASSERT_TRUE(gatt_server);
    exec_req_.device_address = device_address;
    exec_req_.id = request_id;
    exec_req_.is_exec = is_execute;
    exec_req_.count++;
  }

  const RequestData& char_read_req() const { return char_read_req_; }
  const RequestData& desc_read_req() const { return desc_read_req_; }
  const RequestData& char_write_req() const { return char_write_req_; }
  const RequestData& desc_write_req() const { return desc_write_req_; }

 private:
  RequestData char_read_req_;
  RequestData desc_read_req_;
  RequestData char_write_req_;
  RequestData desc_write_req_;
  RequestData exec_req_;
};

class GattServerTest : public ::testing::Test {
 public:
  GattServerTest() = default;
  ~GattServerTest() override = default;

  void SetUp() override {
    mock_handler_.reset(new MockGattHandler());
    fake_hal_gatt_iface_ = new hal::FakeBluetoothGattInterface(
        nullptr,
        std::static_pointer_cast<
            hal::FakeBluetoothGattInterface::TestServerHandler>(mock_handler_));

    hal::BluetoothGattInterface::InitializeForTesting(fake_hal_gatt_iface_);
    factory_.reset(new GattServerFactory());
  }

  void TearDown() override {
    factory_.reset();
    hal::BluetoothGattInterface::CleanUp();
  }

 protected:
  hal::FakeBluetoothGattInterface* fake_hal_gatt_iface_;
  std::shared_ptr<MockGattHandler> mock_handler_;
  std::unique_ptr<GattServerFactory> factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GattServerTest);
};

const int kDefaultServerId = 4;

class GattServerPostRegisterTest : public GattServerTest {
 public:
  GattServerPostRegisterTest() = default;
  ~GattServerPostRegisterTest() override = default;

  void SetUp() override {
    GattServerTest::SetUp();
    UUID uuid = UUID::GetRandom();
    auto callback = [&](BLEStatus status, const UUID& in_uuid,
                        std::unique_ptr<BluetoothInstance> in_client) {
      CHECK(in_uuid == uuid);
      CHECK(in_client.get());
      CHECK(status == BLE_STATUS_SUCCESS);

      gatt_server_ = std::unique_ptr<GattServer>(
          static_cast<GattServer*>(in_client.release()));
    };

    EXPECT_CALL(*mock_handler_, RegisterServer(_))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));

    factory_->RegisterInstance(uuid, callback);

    bt_uuid_t hal_uuid = uuid.GetBlueDroid();
    fake_hal_gatt_iface_->NotifyRegisterServerCallback(
        BT_STATUS_SUCCESS,
        kDefaultServerId,
        hal_uuid);
  }

  void TearDown() override {
    EXPECT_CALL(*mock_handler_, UnregisterServer(_))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));
    gatt_server_ = nullptr;
    GattServerTest::TearDown();
  }

  void SetUpTestService() {
    EXPECT_CALL(*mock_handler_, AddService(_, _, _))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));
    EXPECT_CALL(*mock_handler_, AddCharacteristic(_, _, _, _, _))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));
    EXPECT_CALL(*mock_handler_, AddDescriptor(_, _, _, _))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));
    EXPECT_CALL(*mock_handler_, StartService(_, _, _))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));

    UUID uuid0 = UUID::GetRandom();
    UUID uuid1 = UUID::GetRandom();
    UUID uuid2 = UUID::GetRandom();

    bool register_success = false;

    // Doesn't matter what the permissions/properties are since this is all
    // fake.
    test_service_id_ = *gatt_server_->BeginServiceDeclaration(uuid0, true);
    test_char_id_ = *gatt_server_->AddCharacteristic(uuid1, 0, 0);
    test_desc_id_ = *gatt_server_->AddDescriptor(uuid2, 0);
    ASSERT_TRUE(gatt_server_->EndServiceDeclaration([&](
        BLEStatus status, const GattIdentifier& gatt_id) {
      ASSERT_EQ(BLE_STATUS_SUCCESS, status);
      ASSERT_TRUE(gatt_id == test_service_id_);
      register_success = true;
    }));

    btgatt_srvc_id_t hal_srvc_id;
    hal::GetHALServiceId(test_service_id_, &hal_srvc_id);
    bt_uuid_t hal_uuid1 = uuid1.GetBlueDroid();
    bt_uuid_t hal_uuid2 = uuid2.GetBlueDroid();

    srvc_handle_ = 0x0001;
    char_handle_ = 0x0003;
    desc_handle_ = 0x0004;

    fake_hal_gatt_iface_->NotifyServiceAddedCallback(
        BT_STATUS_SUCCESS, kDefaultServerId, hal_srvc_id, srvc_handle_);
    fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
        BT_STATUS_SUCCESS, kDefaultServerId, hal_uuid1,
        srvc_handle_, char_handle_);
    fake_hal_gatt_iface_->NotifyDescriptorAddedCallback(
        BT_STATUS_SUCCESS, kDefaultServerId, hal_uuid2,
        srvc_handle_, desc_handle_);
    fake_hal_gatt_iface_->NotifyServiceStartedCallback(
        BT_STATUS_SUCCESS, kDefaultServerId, srvc_handle_);

    testing::Mock::VerifyAndClearExpectations(mock_handler_.get());

    ASSERT_TRUE(register_success);
  }

 protected:
  std::unique_ptr<GattServer> gatt_server_;

  GattIdentifier test_service_id_;
  GattIdentifier test_char_id_;
  GattIdentifier test_desc_id_;
  int srvc_handle_;
  int char_handle_;
  int desc_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GattServerPostRegisterTest);
};

TEST_F(GattServerTest, RegisterServer) {
  EXPECT_CALL(*mock_handler_, RegisterServer(_))
      .Times(2)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillOnce(Return(BT_STATUS_SUCCESS));

  // These will be asynchronously populate with a result when the callback
  // executes.
  BLEStatus status = BLE_STATUS_SUCCESS;
  UUID cb_uuid;
  std::unique_ptr<GattServer> server;
  int callback_count = 0;

  auto callback = [&](BLEStatus in_status, const UUID& uuid,
                      std::unique_ptr<BluetoothInstance> in_server) {
    status = in_status;
    cb_uuid = uuid;
    server = std::unique_ptr<GattServer>(
        static_cast<GattServer*>(in_server.release()));
    callback_count++;
  };

  UUID uuid0 = UUID::GetRandom();

  // HAL returns failure.
  EXPECT_FALSE(factory_->RegisterInstance(uuid0, callback));
  EXPECT_EQ(0, callback_count);

  // HAL returns success.
  EXPECT_TRUE(factory_->RegisterInstance(uuid0, callback));
  EXPECT_EQ(0, callback_count);

  // Calling twice with the same UUID should fail with no additional calls into
  // the stack.
  EXPECT_FALSE(factory_->RegisterInstance(uuid0, callback));

  testing::Mock::VerifyAndClearExpectations(mock_handler_.get());

  // Call with a different UUID while one is pending.
  UUID uuid1 = UUID::GetRandom();
  EXPECT_CALL(*mock_handler_, RegisterServer(_))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_TRUE(factory_->RegisterInstance(uuid1, callback));

  // Trigger callback with an unknown UUID. This should get ignored.
  UUID uuid2 = UUID::GetRandom();
  bt_uuid_t hal_uuid = uuid2.GetBlueDroid();
  fake_hal_gatt_iface_->NotifyRegisterServerCallback(0, 0, hal_uuid);
  EXPECT_EQ(0, callback_count);

  // |uuid0| succeeds.
  int server_if0 = 2;  // Pick something that's not 0.
  hal_uuid = uuid0.GetBlueDroid();
  fake_hal_gatt_iface_->NotifyRegisterServerCallback(
      BT_STATUS_SUCCESS, server_if0, hal_uuid);

  EXPECT_EQ(1, callback_count);
  ASSERT_TRUE(server.get() != nullptr);  // Assert to terminate in case of error
  EXPECT_EQ(BLE_STATUS_SUCCESS, status);
  EXPECT_EQ(server_if0, server->GetInstanceId());
  EXPECT_EQ(uuid0, server->GetAppIdentifier());
  EXPECT_EQ(uuid0, cb_uuid);

  // The server should unregister itself when deleted.
  EXPECT_CALL(*mock_handler_, UnregisterServer(server_if0))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  server.reset();

  testing::Mock::VerifyAndClearExpectations(mock_handler_.get());

  // |uuid1| fails.
  int server_if1 = 3;
  hal_uuid = uuid1.GetBlueDroid();
  fake_hal_gatt_iface_->NotifyRegisterServerCallback(
      BT_STATUS_FAIL, server_if1, hal_uuid);

  EXPECT_EQ(2, callback_count);
  ASSERT_TRUE(server.get() == nullptr);  // Assert to terminate in case of error
  EXPECT_EQ(BLE_STATUS_FAILURE, status);
  EXPECT_EQ(uuid1, cb_uuid);
}

TEST_F(GattServerPostRegisterTest, SimpleServiceTest) {
  // Setup a service callback.
  GattIdentifier cb_id;
  BLEStatus cb_status = BLE_STATUS_SUCCESS;
  int cb_count = 0;
  auto callback = [&](BLEStatus in_status, const GattIdentifier& in_id) {
    cb_id = in_id;
    cb_status = in_status;
    cb_count++;
  };

  // Service declaration not started.
  EXPECT_FALSE(gatt_server_->EndServiceDeclaration(callback));

  const UUID uuid = UUID::GetRandom();
  auto service_id = gatt_server_->BeginServiceDeclaration(uuid, true);
  EXPECT_TRUE(service_id != nullptr);
  EXPECT_TRUE(service_id->IsService());

  // Already started.
  EXPECT_FALSE(gatt_server_->BeginServiceDeclaration(uuid, false));

  // Callback is NULL.
  EXPECT_FALSE(
      gatt_server_->EndServiceDeclaration(GattServer::ResultCallback()));

  // We should get a call for a service with one handle.
  EXPECT_CALL(*mock_handler_, AddService(gatt_server_->GetInstanceId(), _, 1))
      .Times(2)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillOnce(Return(BT_STATUS_SUCCESS));

  // Stack returns failure. This will cause the entire service declaration to
  // end and needs to be restarted.
  EXPECT_FALSE(gatt_server_->EndServiceDeclaration(callback));

  service_id = gatt_server_->BeginServiceDeclaration(uuid, true);
  EXPECT_TRUE(service_id != nullptr);
  EXPECT_TRUE(service_id->IsService());

  // Stack returns success.
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  // EndServiceDeclaration already in progress.
  EXPECT_FALSE(gatt_server_->EndServiceDeclaration(callback));

  EXPECT_EQ(0, cb_count);

  btgatt_srvc_id_t hal_id;
  hal::GetHALServiceId(*service_id, &hal_id);
  int srvc_handle = 0x0001;

  // Report success for AddService but for wrong server. Should be ignored.
  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId + 1, hal_id, srvc_handle);
  EXPECT_EQ(0, cb_count);

  // Report success for AddService.
  EXPECT_CALL(*mock_handler_, StartService(kDefaultServerId, srvc_handle, _))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(0, cb_count);

  // Report success for StartService but for wrong server. Should be ignored.
  fake_hal_gatt_iface_->NotifyServiceStartedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId + 1, srvc_handle);
  EXPECT_EQ(0, cb_count);

  // Report success for StartService.
  fake_hal_gatt_iface_->NotifyServiceStartedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, srvc_handle);
  EXPECT_EQ(1, cb_count);
  EXPECT_EQ(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Start new service declaration with same UUID. We should get a different ID.
  auto service_id1 = gatt_server_->BeginServiceDeclaration(uuid, true);
  EXPECT_TRUE(service_id1 != nullptr);
  EXPECT_TRUE(service_id1->IsService());
  EXPECT_TRUE(*service_id != *service_id1);
}

TEST_F(GattServerPostRegisterTest, AddServiceFailures) {
  // Setup a service callback.
  GattIdentifier cb_id;
  BLEStatus cb_status = BLE_STATUS_SUCCESS;
  int cb_count = 0;
  auto callback = [&](BLEStatus in_status, const GattIdentifier& in_id) {
    cb_id = in_id;
    cb_status = in_status;
    cb_count++;
  };

  const UUID uuid = UUID::GetRandom();
  auto service_id = gatt_server_->BeginServiceDeclaration(uuid, true);
  btgatt_srvc_id_t hal_id;
  hal::GetHALServiceId(*service_id, &hal_id);
  int srvc_handle = 0x0001;

  EXPECT_CALL(*mock_handler_, AddService(gatt_server_->GetInstanceId(), _, 1))
      .Times(3)
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  // Report failure for AddService.
  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_FAIL, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(1, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart. We should get the same ID back.
  auto service_id1 = gatt_server_->BeginServiceDeclaration(uuid, true);
  EXPECT_TRUE(*service_id1 == *service_id);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  // Report success for AddService but return failure from StartService.
  EXPECT_CALL(*mock_handler_, StartService(gatt_server_->GetInstanceId(), 1, _))
      .Times(2)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillOnce(Return(BT_STATUS_SUCCESS));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(2, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart.
  service_id = gatt_server_->BeginServiceDeclaration(uuid, true);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  // Report success for AddService, return success from StartService.
  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(2, cb_count);

  // Report failure for StartService. Added service data should get deleted.
  EXPECT_CALL(*mock_handler_,
              DeleteService(gatt_server_->GetInstanceId(), srvc_handle))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  fake_hal_gatt_iface_->NotifyServiceStartedCallback(
      BT_STATUS_FAIL, kDefaultServerId, srvc_handle);
  EXPECT_EQ(3, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);
}

TEST_F(GattServerPostRegisterTest, AddCharacteristic) {
  // Just pick some values.
  const int props = bluetooth::kCharacteristicPropertyRead |
      bluetooth::kCharacteristicPropertyNotify;
  const int perms = kAttributePermissionReadEncrypted;
  const UUID char_uuid = UUID::GetRandom();
  bt_uuid_t hal_char_uuid = char_uuid.GetBlueDroid();

  // Declaration not started.
  EXPECT_EQ(nullptr, gatt_server_->AddCharacteristic(char_uuid, props, perms));

  // Start a service declaration.
  const UUID service_uuid = UUID::GetRandom();
  auto service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  EXPECT_TRUE(service_id != nullptr);
  btgatt_srvc_id_t hal_id;
  hal::GetHALServiceId(*service_id, &hal_id);

  // Add two characteristics with the same UUID.
  auto char_id0 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  auto char_id1 = gatt_server_->AddCharacteristic(char_uuid, props, perms);

  EXPECT_TRUE(char_id0 != nullptr);
  EXPECT_TRUE(char_id1 != nullptr);
  EXPECT_TRUE(char_id0 != char_id1);
  EXPECT_TRUE(char_id0->IsCharacteristic());
  EXPECT_TRUE(char_id1->IsCharacteristic());
  EXPECT_TRUE(*char_id0->GetOwningServiceId() == *service_id);
  EXPECT_TRUE(*char_id1->GetOwningServiceId() == *service_id);

  // Expect calls for 5 handles in total as we have 2 characteristics.
  EXPECT_CALL(*mock_handler_, AddService(kDefaultServerId, _, 5))
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));

  GattIdentifier cb_id;
  BLEStatus cb_status;
  int cb_count = 0;
  auto callback = [&](BLEStatus in_status, const GattIdentifier& in_id) {
    cb_id = in_id;
    cb_status = in_status;
    cb_count++;
  };

  int srvc_handle = 0x0001;
  int char_handle0 = 0x0002;
  int char_handle1 = 0x0004;
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  // Cannot add any more characteristics while EndServiceDeclaration is in
  // progress.
  EXPECT_EQ(nullptr, gatt_server_->AddCharacteristic(char_uuid, props, perms));

  EXPECT_CALL(*mock_handler_, AddCharacteristic(_, _, _, _, _))
      .Times(8)
      .WillOnce(Return(BT_STATUS_FAIL))      // char_id0 - try 1
      .WillOnce(Return(BT_STATUS_SUCCESS))   // char_id0 - try 2
      .WillOnce(Return(BT_STATUS_SUCCESS))   // char_id0 - try 3
      .WillOnce(Return(BT_STATUS_FAIL))      // char_id1 - try 3
      .WillOnce(Return(BT_STATUS_SUCCESS))   // char_id0 - try 4
      .WillOnce(Return(BT_STATUS_SUCCESS))   // char_id1 - try 4
      .WillOnce(Return(BT_STATUS_SUCCESS))   // char_id0 - try 5
      .WillOnce(Return(BT_STATUS_SUCCESS));  // char_id1 - try 5

  // First AddCharacteristic call will fail.
  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(1, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart. (try 2)
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  hal::GetHALServiceId(*service_id, &hal_id);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(1, cb_count);

  // Report failure for pending AddCharacteristic.
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_FAIL, kDefaultServerId, hal_char_uuid,
      srvc_handle, char_handle0);
  EXPECT_EQ(2, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart. (try 3)
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  hal::GetHALServiceId(*service_id, &hal_id);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(2, cb_count);

  // Report success for pending AddCharacteristic we should receive a call for
  // the second characteristic which will fail.
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid,
      srvc_handle, char_handle0);
  EXPECT_EQ(3, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart. (try 4)
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  hal::GetHALServiceId(*service_id, &hal_id);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(3, cb_count);

  // Report success for pending AddCharacteristic. Second characteristic call
  // will start normally. We shouldn't receive any new callback.
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid,
      srvc_handle, char_handle0);
  EXPECT_EQ(3, cb_count);

  // Report failure for pending AddCharacteristic call for second
  // characteristic.
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_FAIL, kDefaultServerId, hal_char_uuid,
      srvc_handle, char_handle1);
  EXPECT_EQ(4, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart. (try 5)
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid, props, perms);
  hal::GetHALServiceId(*service_id, &hal_id);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(4, cb_count);

  // Report success for pending AddCharacteristic. Second characteristic call
  // will start normally. We shouldn't receive any new callback.
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid,
      srvc_handle, char_handle0);
  EXPECT_EQ(4, cb_count);

  // Report success for pending AddCharacteristic call for second
  // characteristic. We shouldn't receive any new callback but we'll get a call
  // to StartService.
  EXPECT_CALL(*mock_handler_, StartService(kDefaultServerId, srvc_handle, _))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid,
      srvc_handle, char_handle1);
  EXPECT_EQ(4, cb_count);
}

TEST_F(GattServerPostRegisterTest, AddDescriptor) {
  // Set up some values for UUIDs, permissions, and properties.
  const UUID service_uuid = UUID::GetRandom();
  const UUID char_uuid0 = UUID::GetRandom();
  const UUID char_uuid1 = UUID::GetRandom();
  const UUID desc_uuid = UUID::GetRandom();
  bt_uuid_t hal_char_uuid0 = char_uuid0.GetBlueDroid();
  bt_uuid_t hal_char_uuid1 = char_uuid1.GetBlueDroid();
  bt_uuid_t hal_desc_uuid = desc_uuid.GetBlueDroid();
  const int props = bluetooth::kCharacteristicPropertyRead |
      bluetooth::kCharacteristicPropertyNotify;
  const int perms = kAttributePermissionReadEncrypted;

  // Service declaration not started.
  EXPECT_EQ(nullptr, gatt_server_->AddDescriptor(desc_uuid, perms));

  // Start a service declaration.
  auto service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  btgatt_srvc_id_t hal_id;
  hal::GetHALServiceId(*service_id, &hal_id);

  // No characteristic was inserted.
  EXPECT_EQ(nullptr, gatt_server_->AddDescriptor(desc_uuid, perms));

  // Add two characeristics.
  auto char_id0 = gatt_server_->AddCharacteristic(char_uuid0, props, perms);
  auto char_id1 = gatt_server_->AddCharacteristic(char_uuid1, props, perms);

  // Add a descriptor.
  auto desc_id = gatt_server_->AddDescriptor(desc_uuid, perms);
  EXPECT_NE(nullptr, desc_id);
  EXPECT_TRUE(desc_id->IsDescriptor());
  EXPECT_TRUE(*desc_id->GetOwningCharacteristicId() == *char_id1);
  EXPECT_TRUE(*desc_id->GetOwningServiceId() == *service_id);

  // Add a second descriptor with the same UUID.
  auto desc_id1 = gatt_server_->AddDescriptor(desc_uuid, perms);
  EXPECT_NE(nullptr, desc_id1);
  EXPECT_TRUE(*desc_id1 != *desc_id);
  EXPECT_TRUE(desc_id1->IsDescriptor());
  EXPECT_TRUE(*desc_id1->GetOwningCharacteristicId() == *char_id1);
  EXPECT_TRUE(*desc_id1->GetOwningServiceId() == *service_id);

  // Expect calls for 7 handles.
  EXPECT_CALL(*mock_handler_, AddService(kDefaultServerId, _, 7))
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));
  EXPECT_CALL(*mock_handler_, AddCharacteristic(_, _, _, _, _))
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));

  GattIdentifier cb_id;
  BLEStatus cb_status;
  int cb_count = 0;
  auto callback = [&](BLEStatus in_status, const GattIdentifier& in_id) {
    cb_id = in_id;
    cb_status = in_status;
    cb_count++;
  };

  int srvc_handle = 0x0001;
  int char_handle0 = 0x0002;
  int char_handle1 = 0x0004;
  int desc_handle0 = 0x0005;
  int desc_handle1 = 0x0006;

  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  // Cannot add any more descriptors while EndServiceDeclaration is in progress.
  EXPECT_EQ(nullptr, gatt_server_->AddDescriptor(desc_uuid, perms));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(0, cb_count);

  EXPECT_CALL(*mock_handler_, AddDescriptor(_, _, _, _))
      .Times(8)
      .WillOnce(Return(BT_STATUS_FAIL))      // desc_id0 - try 1
      .WillOnce(Return(BT_STATUS_SUCCESS))   // desc_id0 - try 2
      .WillOnce(Return(BT_STATUS_SUCCESS))   // desc_id0 - try 3
      .WillOnce(Return(BT_STATUS_FAIL))      // desc_id1 - try 3
      .WillOnce(Return(BT_STATUS_SUCCESS))   // desc_id0 - try 4
      .WillOnce(Return(BT_STATUS_SUCCESS))   // desc_id1 - try 4
      .WillOnce(Return(BT_STATUS_SUCCESS))   // desc_id0 - try 5
      .WillOnce(Return(BT_STATUS_SUCCESS));  // desc_id1 - try 5

  // Notify success for both characteristics. First descriptor call will fail.
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid0,
      srvc_handle, char_handle0);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid1,
      srvc_handle, char_handle1);
  EXPECT_EQ(1, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart (try 2)
  cb_count = 0;
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  hal::GetHALServiceId(*service_id, &hal_id);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid0, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid1, props, perms);
  desc_id = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id);
  desc_id1 = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id1);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid0,
      srvc_handle, char_handle0);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid1,
      srvc_handle, char_handle1);
  EXPECT_EQ(0, cb_count);

  // Notify failure for first descriptor.
  fake_hal_gatt_iface_->NotifyDescriptorAddedCallback(
      BT_STATUS_FAIL, kDefaultServerId, hal_desc_uuid,
      srvc_handle, desc_handle0);
  EXPECT_EQ(1, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart (try 3)
  cb_count = 0;
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  hal::GetHALServiceId(*service_id, &hal_id);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid0, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid1, props, perms);
  desc_id = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id);
  desc_id1 = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id1);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid0,
      srvc_handle, char_handle0);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid1,
      srvc_handle, char_handle1);
  EXPECT_EQ(0, cb_count);

  // Notify success for first descriptor; the second descriptor will fail
  // immediately.
  fake_hal_gatt_iface_->NotifyDescriptorAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_desc_uuid,
      srvc_handle, desc_handle0);
  EXPECT_EQ(1, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart (try 4)
  cb_count = 0;
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  hal::GetHALServiceId(*service_id, &hal_id);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid0, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid1, props, perms);
  desc_id = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id);
  desc_id1 = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id1);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid0,
      srvc_handle, char_handle0);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid1,
      srvc_handle, char_handle1);
  EXPECT_EQ(0, cb_count);

  // Notify success for first first descriptor and failure for second
  // descriptor.
  fake_hal_gatt_iface_->NotifyDescriptorAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_desc_uuid,
      srvc_handle, desc_handle0);
  EXPECT_EQ(0, cb_count);

  fake_hal_gatt_iface_->NotifyDescriptorAddedCallback(
      BT_STATUS_FAIL, kDefaultServerId, hal_desc_uuid,
      srvc_handle, desc_handle1);
  EXPECT_EQ(1, cb_count);
  EXPECT_NE(BLE_STATUS_SUCCESS, cb_status);
  EXPECT_TRUE(cb_id == *service_id);

  // Restart (try 5)
  cb_count = 0;
  service_id = gatt_server_->BeginServiceDeclaration(service_uuid, true);
  hal::GetHALServiceId(*service_id, &hal_id);
  char_id0 = gatt_server_->AddCharacteristic(char_uuid0, props, perms);
  char_id1 = gatt_server_->AddCharacteristic(char_uuid1, props, perms);
  desc_id = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id);
  desc_id1 = gatt_server_->AddDescriptor(desc_uuid, perms);
  ASSERT_NE(nullptr, desc_id1);
  EXPECT_TRUE(gatt_server_->EndServiceDeclaration(callback));

  fake_hal_gatt_iface_->NotifyServiceAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_id, srvc_handle);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid0,
      srvc_handle, char_handle0);
  EXPECT_EQ(0, cb_count);
  fake_hal_gatt_iface_->NotifyCharacteristicAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_char_uuid1,
      srvc_handle, char_handle1);
  EXPECT_EQ(0, cb_count);

  // Notify success for both descriptors.
  fake_hal_gatt_iface_->NotifyDescriptorAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_desc_uuid,
      srvc_handle, desc_handle0);
  EXPECT_EQ(0, cb_count);

  // The second descriptor callback should trigger the end routine.
  EXPECT_CALL(*mock_handler_, StartService(kDefaultServerId, srvc_handle, _))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  fake_hal_gatt_iface_->NotifyDescriptorAddedCallback(
      BT_STATUS_SUCCESS, kDefaultServerId, hal_desc_uuid,
      srvc_handle, desc_handle1);
  EXPECT_EQ(0, cb_count);
}

TEST_F(GattServerPostRegisterTest, RequestRead) {
  SetUpTestService();

  TestDelegate test_delegate;
  gatt_server_->SetDelegate(&test_delegate);

  const std::vector<uint8_t> kTestValue = { 0x01, 0x02, 0x03 };
  const std::vector<uint8_t> kTestValueTooLarge(BTGATT_MAX_ATTR_LEN + 1, 0);
  const std::string kTestAddress0 = "01:23:45:67:89:AB";
  const std::string kTestAddress1 = "CD:EF:01:23:45:67";
  const int kReqId0 = 0;
  const int kReqId1 = 1;
  const int kConnId0 = 1;

  // No pending request.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  bt_bdaddr_t hal_addr0, hal_addr1;
  ASSERT_TRUE(util::BdAddrFromString(kTestAddress0, &hal_addr0));
  ASSERT_TRUE(util::BdAddrFromString(kTestAddress1, &hal_addr1));

  // Send a connection callback. The GattServer should store the connection
  // information and be able to process the incoming read requests for this
  // connection.
  fake_hal_gatt_iface_->NotifyServerConnectionCallback(
      kConnId0, kDefaultServerId, true, hal_addr0);

  // Unknown connection ID shouldn't trigger anything.
  fake_hal_gatt_iface_->NotifyRequestReadCallback(
      kConnId0 + 1, kReqId0, hal_addr0, char_handle_, 0, false);
  EXPECT_EQ(0, test_delegate.char_read_req().count);
  EXPECT_EQ(0, test_delegate.desc_read_req().count);

  // Unknown device address shouldn't trigger anything.
  fake_hal_gatt_iface_->NotifyRequestReadCallback(
      kConnId0, kReqId0, hal_addr1, char_handle_, 0, false);
  EXPECT_EQ(0, test_delegate.char_read_req().count);
  EXPECT_EQ(0, test_delegate.desc_read_req().count);

  // Unknown attribute handle shouldn't trigger anything.
  fake_hal_gatt_iface_->NotifyRequestReadCallback(
      kConnId0, kReqId0, hal_addr0, char_handle_ + 50, 0, false);
  EXPECT_EQ(0, test_delegate.char_read_req().count);
  EXPECT_EQ(0, test_delegate.desc_read_req().count);

  // Characteristic and descriptor handles should trigger correct callbacks.
  fake_hal_gatt_iface_->NotifyRequestReadCallback(
      kConnId0, kReqId0, hal_addr0, char_handle_, 0, false);
  EXPECT_EQ(1, test_delegate.char_read_req().count);
  EXPECT_EQ(kTestAddress0, test_delegate.char_read_req().device_address);
  EXPECT_EQ(kReqId0, test_delegate.char_read_req().id);
  EXPECT_EQ(0, test_delegate.char_read_req().offset);
  EXPECT_FALSE(test_delegate.char_read_req().is_long);
  EXPECT_TRUE(test_char_id_ == test_delegate.char_read_req().gatt_id);
  EXPECT_EQ(0, test_delegate.desc_read_req().count);

  fake_hal_gatt_iface_->NotifyRequestReadCallback(
      kConnId0, kReqId1, hal_addr0, desc_handle_, 2, true);
  EXPECT_EQ(1, test_delegate.char_read_req().count);
  EXPECT_EQ(1, test_delegate.desc_read_req().count);
  EXPECT_EQ(kTestAddress0, test_delegate.desc_read_req().device_address);
  EXPECT_EQ(kReqId1, test_delegate.desc_read_req().id);
  EXPECT_EQ(2, test_delegate.desc_read_req().offset);
  EXPECT_TRUE(test_delegate.desc_read_req().is_long);
  EXPECT_TRUE(test_desc_id_ == test_delegate.desc_read_req().gatt_id);

  // Callback with a pending request ID will be ignored.
  fake_hal_gatt_iface_->NotifyRequestReadCallback(
      kConnId0, kReqId0, hal_addr0, char_handle_, 0, false);
  fake_hal_gatt_iface_->NotifyRequestReadCallback(
      kConnId0, kReqId1, hal_addr0, char_handle_, 0, false);
  EXPECT_EQ(1, test_delegate.char_read_req().count);
  EXPECT_EQ(1, test_delegate.desc_read_req().count);

  // Send response for wrong device address.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress1, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  // Send response for a value that's too large.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValueTooLarge));

  EXPECT_CALL(*mock_handler_, SendResponse(kConnId0, kReqId0,
                                           BT_STATUS_SUCCESS, _))
      .Times(2)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillOnce(Return(BT_STATUS_SUCCESS));

  // Stack call fails.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  // Successful send response for characteristic.
  EXPECT_TRUE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  // Characteristic request ID no longer pending.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  EXPECT_CALL(*mock_handler_, SendResponse(kConnId0, kReqId1,
                                           BT_STATUS_SUCCESS, _))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));

  // Successful send response for descriptor.
  EXPECT_TRUE(gatt_server_->SendResponse(
      kTestAddress0, kReqId1,
      GATT_ERROR_NONE, 0, kTestValue));

  // Descriptor request ID no longer pending.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId1,
      GATT_ERROR_NONE, 0, kTestValue));

  gatt_server_->SetDelegate(nullptr);
}

TEST_F(GattServerPostRegisterTest, RequestWrite) {
  SetUpTestService();

  TestDelegate test_delegate;
  gatt_server_->SetDelegate(&test_delegate);

  const std::vector<uint8_t> kTestValue = { 0x01, 0x02, 0x03 };
  const std::string kTestAddress0 = "01:23:45:67:89:AB";
  const std::string kTestAddress1 = "CD:EF:01:23:45:67";
  const int kReqId0 = 0;
  const int kReqId1 = 1;
  const int kConnId0 = 1;

  // No pending request.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  bt_bdaddr_t hal_addr0, hal_addr1;
  ASSERT_TRUE(util::BdAddrFromString(kTestAddress0, &hal_addr0));
  ASSERT_TRUE(util::BdAddrFromString(kTestAddress1, &hal_addr1));

  // Send a connection callback. The GattServer should store the connection
  // information and be able to process the incoming read requests for this
  // connection.
  fake_hal_gatt_iface_->NotifyServerConnectionCallback(
      kConnId0, kDefaultServerId, true, hal_addr0);

  // Unknown connection ID shouldn't trigger anything.
  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0 + 1, kReqId0, hal_addr0, char_handle_, 0,
      kTestValue.size(), true, false, (uint8_t *)kTestValue.data());
  EXPECT_EQ(0, test_delegate.char_write_req().count);
  EXPECT_EQ(0, test_delegate.desc_write_req().count);

  // Unknown device address shouldn't trigger anything.
  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0, kReqId0, hal_addr1, char_handle_, 0,
      kTestValue.size(), true, false, (uint8_t *)kTestValue.data());
  EXPECT_EQ(0, test_delegate.char_write_req().count);
  EXPECT_EQ(0, test_delegate.desc_write_req().count);

  // Unknown attribute handle shouldn't trigger anything.
  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0, kReqId0, hal_addr0, char_handle_ + 50, 0,
      kTestValue.size(), true, false, (uint8_t *)kTestValue.data());
  EXPECT_EQ(0, test_delegate.char_write_req().count);
  EXPECT_EQ(0, test_delegate.desc_write_req().count);

  // Characteristic and descriptor handles should trigger correct callbacks.
  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0, kReqId0, hal_addr0, char_handle_, 0,
      kTestValue.size(), true, false, (uint8_t *)kTestValue.data());
  EXPECT_EQ(1, test_delegate.char_write_req().count);
  EXPECT_EQ(kTestAddress0, test_delegate.char_write_req().device_address);
  EXPECT_EQ(kReqId0, test_delegate.char_write_req().id);
  EXPECT_EQ(0, test_delegate.char_write_req().offset);
  EXPECT_EQ(true, test_delegate.char_write_req().need_rsp);
  EXPECT_EQ(false, test_delegate.char_write_req().is_exec);
  EXPECT_EQ(kTestValue, test_delegate.char_write_req().write_value);
  EXPECT_TRUE(test_char_id_ == test_delegate.char_write_req().gatt_id);
  EXPECT_EQ(0, test_delegate.desc_write_req().count);

  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0, kReqId1, hal_addr0, desc_handle_, 2,
      kTestValue.size(), true, false, (uint8_t *)kTestValue.data());
  EXPECT_EQ(1, test_delegate.char_write_req().count);
  EXPECT_EQ(1, test_delegate.desc_write_req().count);
  EXPECT_EQ(kTestAddress0, test_delegate.desc_write_req().device_address);
  EXPECT_EQ(kReqId1, test_delegate.desc_write_req().id);
  EXPECT_EQ(2, test_delegate.desc_write_req().offset);
  EXPECT_EQ(true, test_delegate.desc_write_req().need_rsp);
  EXPECT_EQ(false, test_delegate.desc_write_req().is_exec);
  EXPECT_EQ(kTestValue, test_delegate.desc_write_req().write_value);
  EXPECT_TRUE(test_desc_id_ == test_delegate.desc_write_req().gatt_id);

  // Callback with a pending request ID will be ignored.
  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0, kReqId0, hal_addr0, char_handle_, 0,
      kTestValue.size(), true, false, (uint8_t *)kTestValue.data());
  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0, kReqId1, hal_addr0, char_handle_, 0,
      kTestValue.size(), true, false, (uint8_t *)kTestValue.data());
  EXPECT_EQ(1, test_delegate.char_write_req().count);
  EXPECT_EQ(1, test_delegate.desc_write_req().count);

  // Send response for wrong device address.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress1, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  EXPECT_CALL(*mock_handler_, SendResponse(kConnId0, kReqId0,
                                           BT_STATUS_SUCCESS, _))
      .Times(2)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillOnce(Return(BT_STATUS_SUCCESS));

  // Stack call fails.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  // Successful send response for characteristic.
  EXPECT_TRUE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  // Characteristic request ID no longer pending.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  EXPECT_CALL(*mock_handler_, SendResponse(kConnId0, kReqId1,
                                           BT_STATUS_SUCCESS, _))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));

  // Successful send response for descriptor.
  EXPECT_TRUE(gatt_server_->SendResponse(
      kTestAddress0, kReqId1,
      GATT_ERROR_NONE, 0, kTestValue));

  // Descriptor request ID no longer pending.
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId1,
      GATT_ERROR_NONE, 0, kTestValue));

  // SendResponse should fail for a "Write Without Response".
  fake_hal_gatt_iface_->NotifyRequestWriteCallback(
      kConnId0, kReqId0, hal_addr0, char_handle_, 0,
      kTestValue.size(), false, false, (uint8_t *)kTestValue.data());
  EXPECT_EQ(false, test_delegate.char_write_req().need_rsp);
  EXPECT_FALSE(gatt_server_->SendResponse(
      kTestAddress0, kReqId0,
      GATT_ERROR_NONE, 0, kTestValue));

  gatt_server_->SetDelegate(nullptr);
}

TEST_F(GattServerPostRegisterTest, SendNotification) {
  SetUpTestService();

  const std::string kTestAddress0 = "01:23:45:67:89:AB";
  const std::string kTestAddress1 = "cd:ef:01:23:45:67";
  const std::string kInvalidAddress = "thingamajig blabbidyboop";
  const int kConnId0 = 0;
  const int kConnId1 = 1;
  std::vector<uint8_t> value;
  bt_bdaddr_t hal_addr0;
  ASSERT_TRUE(util::BdAddrFromString(kTestAddress0, &hal_addr0));

  // Set up two connections with the same address.
  fake_hal_gatt_iface_->NotifyServerConnectionCallback(
      kConnId0, kDefaultServerId, true, hal_addr0);
  fake_hal_gatt_iface_->NotifyServerConnectionCallback(
      kConnId1, kDefaultServerId, true, hal_addr0);

  // Set up a test callback.
  GATTError gatt_error;
  int callback_count = 0;
  auto callback = [&](GATTError in_error) {
    gatt_error = in_error;
    callback_count++;
  };

  // Bad device address.
  EXPECT_FALSE(gatt_server_->SendNotification(
      kInvalidAddress,
      test_char_id_, false, value, callback));

  // Bad connection.
  EXPECT_FALSE(gatt_server_->SendNotification(
      kTestAddress1,
      test_char_id_, false, value, callback));

  // We should get a HAL call for each connection for this address. The calls
  // fail.
  EXPECT_CALL(*mock_handler_,
              SendIndication(kDefaultServerId, char_handle_, kConnId0,
                             value.size(), 0, nullptr))
      .Times(1)
      .WillOnce(Return(BT_STATUS_FAIL));
  EXPECT_CALL(*mock_handler_,
              SendIndication(kDefaultServerId, char_handle_, kConnId1,
                             value.size(), 0, nullptr))
      .Times(1)
      .WillOnce(Return(BT_STATUS_FAIL));
  EXPECT_FALSE(gatt_server_->SendNotification(
      kTestAddress0,
      test_char_id_, false, value, callback));

  // One of the calls succeeds.
  EXPECT_CALL(*mock_handler_,
              SendIndication(kDefaultServerId, char_handle_, kConnId0,
                             value.size(), 0, nullptr))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_CALL(*mock_handler_,
              SendIndication(kDefaultServerId, char_handle_, kConnId1,
                             value.size(), 0, nullptr))
      .Times(1)
      .WillOnce(Return(BT_STATUS_FAIL));
  EXPECT_TRUE(gatt_server_->SendNotification(
      kTestAddress0,
      test_char_id_, false, value, callback));

  // One of the connections is already pending so there should be only one call.
  // This one we send with confirm=true.
  EXPECT_CALL(*mock_handler_,
              SendIndication(kDefaultServerId, char_handle_, kConnId1,
                             value.size(), 1, nullptr))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_TRUE(gatt_server_->SendNotification(
      kTestAddress0,
      test_char_id_, true, value, callback));

  // Calls are already pending.
  EXPECT_FALSE(gatt_server_->SendNotification(
      kTestAddress0, test_char_id_, true, value, callback));

  // Trigger one confirmation callback. We should get calls for two callbacks
  // since we have two separate calls pending.
  fake_hal_gatt_iface_->NotifyIndicationSentCallback(
      kConnId0, BT_STATUS_SUCCESS);
  fake_hal_gatt_iface_->NotifyIndicationSentCallback(
      kConnId1, BT_STATUS_SUCCESS);
  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(GATT_ERROR_NONE, gatt_error);

  callback_count = 0;

  // Restart. Both calls succeed now.
  EXPECT_CALL(*mock_handler_,
              SendIndication(kDefaultServerId, char_handle_, kConnId0,
                             value.size(), 0, nullptr))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_CALL(*mock_handler_,
              SendIndication(kDefaultServerId, char_handle_, kConnId1,
                             value.size(), 0, nullptr))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_TRUE(gatt_server_->SendNotification(
      kTestAddress0,
      test_char_id_, false, value, callback));

  // Trigger one confirmation callback. The callback we passed should still be
  // pending. The first callback is for the wrong connection ID.
  fake_hal_gatt_iface_->NotifyIndicationSentCallback(
      kConnId0 + 50, BT_STATUS_FAIL);
  fake_hal_gatt_iface_->NotifyIndicationSentCallback(
      kConnId0, BT_STATUS_SUCCESS);
  EXPECT_EQ(0, callback_count);

  // This should be ignored since |kConnId0| was already processed.
  fake_hal_gatt_iface_->NotifyIndicationSentCallback(
      kConnId0, BT_STATUS_SUCCESS);
  EXPECT_EQ(0, callback_count);

  // Run the callback with failure. Since the previous callback reported
  // success, we should report success.
  fake_hal_gatt_iface_->NotifyIndicationSentCallback(
      kConnId1, BT_STATUS_SUCCESS);
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(GATT_ERROR_NONE, gatt_error);
}

}  // namespace
}  // namespace bluetooth
