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

#include <base/macros.h>
#include <gtest/gtest.h>

#include "service/adapter.h"
#include "service/common/bluetooth/util/address_helper.h"
#include "service/hal/fake_bluetooth_gatt_interface.h"
#include "service/hal/fake_bluetooth_interface.h"

namespace bluetooth {
namespace {

class AdapterTest : public ::testing::Test {
 public:
  AdapterTest() = default;
  ~AdapterTest() override = default;

  void SetUp() override {
    fake_hal_manager_ = hal::FakeBluetoothInterface::GetManager();
    fake_hal_iface_ = new hal::FakeBluetoothInterface();
    hal::BluetoothInterface::InitializeForTesting(fake_hal_iface_);

    // Initialize GATT interface with default handlers.
    hal::BluetoothGattInterface::InitializeForTesting(
        new hal::FakeBluetoothGattInterface(nullptr, nullptr));

    adapter_ = Adapter::Create();
  }

  void TearDown() override {
    adapter_.reset();
    hal::BluetoothGattInterface::CleanUp();
    hal::BluetoothInterface::CleanUp();
  }

 protected:
  hal::FakeBluetoothInterface* fake_hal_iface_;
  hal::FakeBluetoothInterface::Manager* fake_hal_manager_;
  std::unique_ptr<Adapter> adapter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AdapterTest);
};

class TestObserver final : public bluetooth::Adapter::Observer {
 public:
  TestObserver(bluetooth::Adapter* adapter)
      : adapter_(adapter),
        prev_state_(bluetooth::ADAPTER_STATE_INVALID),
        cur_state_(bluetooth::ADAPTER_STATE_INVALID),
        last_device_connected_state_(false) {
    CHECK(adapter_);
    adapter_->AddObserver(this);
  }

  ~TestObserver() override {
    adapter_->RemoveObserver(this);
  }

  bluetooth::AdapterState prev_state() const { return prev_state_; }
  bluetooth::AdapterState cur_state() const { return cur_state_; }

  std::string last_connection_state_address() const {
    return last_connection_state_address_;
  }

  bool last_device_connected_state() const {
    return last_device_connected_state_;
  }

  // bluetooth::Adapter::Observer override:
  void OnAdapterStateChanged(bluetooth::Adapter* adapter,
                             bluetooth::AdapterState prev_state,
                             bluetooth::AdapterState new_state) override {
    ASSERT_EQ(adapter_, adapter);
    prev_state_ = prev_state;
    cur_state_ = new_state;
  }

  void OnDeviceConnectionStateChanged(
      Adapter* adapter,
      const std::string& device_address,
      bool connected) override {
    ASSERT_EQ(adapter_, adapter);
    last_connection_state_address_ = device_address;
    last_device_connected_state_ = connected;
  }

 private:
  bluetooth::Adapter* adapter_;
  bluetooth::AdapterState prev_state_, cur_state_;
  std::string last_connection_state_address_;
  bool last_device_connected_state_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

TEST_F(AdapterTest, IsEnabled) {
  EXPECT_FALSE(adapter_->IsEnabled());

  fake_hal_iface_->NotifyAdapterStateChanged(BT_STATE_ON);
  EXPECT_TRUE(adapter_->IsEnabled());

  fake_hal_iface_->NotifyAdapterStateChanged(BT_STATE_OFF);
  EXPECT_FALSE(adapter_->IsEnabled());
}

TEST_F(AdapterTest, Enable) {
  TestObserver observer(adapter_.get());

  EXPECT_FALSE(adapter_->IsEnabled());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_OFF, adapter_->GetState());

  // Enable fails at HAL level
  EXPECT_FALSE(adapter_->Enable(false));
  EXPECT_EQ(bluetooth::ADAPTER_STATE_OFF, adapter_->GetState());

  // Enable success
  fake_hal_manager_->enable_succeed = true;
  EXPECT_TRUE(adapter_->Enable(false));

  // Should have received a state update.
  EXPECT_EQ(bluetooth::ADAPTER_STATE_OFF, observer.prev_state());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_TURNING_ON, observer.cur_state());

  // Enable fails because not disabled
  EXPECT_EQ(bluetooth::ADAPTER_STATE_TURNING_ON, adapter_->GetState());
  EXPECT_FALSE(adapter_->Enable(false));

  // Adapter state updates properly
  fake_hal_iface_->NotifyAdapterStateChanged(BT_STATE_ON);
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, adapter_->GetState());

  // Should have received a state update.
  EXPECT_EQ(bluetooth::ADAPTER_STATE_TURNING_ON, observer.prev_state());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, observer.cur_state());

  // Enable fails because already enabled
  EXPECT_FALSE(adapter_->Enable(false));
}

TEST_F(AdapterTest, Disable) {
  TestObserver observer(adapter_.get());

  fake_hal_manager_->disable_succeed = true;
  EXPECT_FALSE(adapter_->IsEnabled());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_OFF, adapter_->GetState());

  // Disable fails because already disabled
  EXPECT_FALSE(adapter_->Disable());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_OFF, adapter_->GetState());

  // Disable success
  fake_hal_iface_->NotifyAdapterStateChanged(BT_STATE_ON);

  // Should have received a state update.
  EXPECT_EQ(bluetooth::ADAPTER_STATE_OFF, observer.prev_state());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, observer.cur_state());

  EXPECT_TRUE(adapter_->Disable());

  // Should have received a state update.
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, observer.prev_state());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_TURNING_OFF, observer.cur_state());

  // Disable fails because not enabled
  EXPECT_EQ(bluetooth::ADAPTER_STATE_TURNING_OFF, adapter_->GetState());
  EXPECT_FALSE(adapter_->Disable());

  fake_hal_iface_->NotifyAdapterStateChanged(BT_STATE_ON);
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, adapter_->GetState());

  // Should have received a state update.
  EXPECT_EQ(bluetooth::ADAPTER_STATE_TURNING_OFF, observer.prev_state());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, observer.cur_state());

  // Disable fails at HAL level
  fake_hal_manager_->disable_succeed = false;
  EXPECT_FALSE(adapter_->Disable());

  // Should have received a state update. In this case we will receive two
  // updates: one going from OFF to TURNING_OFF, and one going from TURNING_OFF
  // back to ON since we failed to initiate the disable operation.
  EXPECT_EQ(bluetooth::ADAPTER_STATE_TURNING_OFF, observer.prev_state());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, observer.cur_state());

  // Update state to OFF. Should receive a state update.
  fake_hal_iface_->NotifyAdapterStateChanged(BT_STATE_OFF);
  EXPECT_EQ(bluetooth::ADAPTER_STATE_ON, observer.prev_state());
  EXPECT_EQ(bluetooth::ADAPTER_STATE_OFF, observer.cur_state());
}

TEST_F(AdapterTest, GetName) {
  EXPECT_EQ(bluetooth::Adapter::kDefaultName, adapter_->GetName());

  const char kTestAdapterName[] = "Test Adapter Name";

  fake_hal_iface_->NotifyAdapterNamePropertyChanged(kTestAdapterName);
  EXPECT_EQ(kTestAdapterName, adapter_->GetName());
}

TEST_F(AdapterTest, SetName) {
  bt_bdname_t hal_name;

  // Name too large.
  EXPECT_FALSE(adapter_->SetName(std::string(sizeof(hal_name.name), 'a')));

  // Valid length.
  EXPECT_FALSE(adapter_->SetName("Test Name"));
  fake_hal_manager_->set_property_succeed = true;
  EXPECT_TRUE(adapter_->SetName("Test Name"));
}

TEST_F(AdapterTest, GetAddress) {
  EXPECT_EQ(bluetooth::Adapter::kDefaultAddress, adapter_->GetAddress());

  const bt_bdaddr_t kTestAdapterInput = {
    { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc }
  };
  const char kTestAdapterAddressOutput[] = "12:34:56:78:9A:BC";

  fake_hal_iface_->NotifyAdapterAddressPropertyChanged(&kTestAdapterInput);
  EXPECT_EQ(kTestAdapterAddressOutput, adapter_->GetAddress());
}

TEST_F(AdapterTest, IsMultiAdvertisementSupported) {
  EXPECT_FALSE(adapter_->IsMultiAdvertisementSupported());

  bt_local_le_features_t features;
  memset(&features, 0, sizeof(features));

  features.max_adv_instance = 10;  // Some high number.
  fake_hal_iface_->NotifyAdapterLocalLeFeaturesPropertyChanged(&features);
  EXPECT_TRUE(adapter_->IsMultiAdvertisementSupported());

  features.max_adv_instance = 0;  // Low number.
  fake_hal_iface_->NotifyAdapterLocalLeFeaturesPropertyChanged(&features);
  EXPECT_FALSE(adapter_->IsMultiAdvertisementSupported());
}

TEST_F(AdapterTest, IsDeviceConnected) {
  const char kDeviceAddr[] = "12:34:56:78:9A:BC";
  TestObserver observer(adapter_.get());

  EXPECT_FALSE(adapter_->IsDeviceConnected(kDeviceAddr));

  bt_bdaddr_t hal_addr;
  ASSERT_TRUE(util::BdAddrFromString(kDeviceAddr, &hal_addr));

  // status != BT_STATUS_SUCCESS should be ignored
  fake_hal_iface_->NotifyAclStateChangedCallback(
      BT_STATUS_FAIL, hal_addr, BT_ACL_STATE_CONNECTED);
  EXPECT_FALSE(adapter_->IsDeviceConnected(kDeviceAddr));
  EXPECT_TRUE(observer.last_connection_state_address().empty());
  EXPECT_FALSE(observer.last_device_connected_state());

  // Connected
  fake_hal_iface_->NotifyAclStateChangedCallback(
      BT_STATUS_SUCCESS, hal_addr, BT_ACL_STATE_CONNECTED);
  EXPECT_TRUE(adapter_->IsDeviceConnected(kDeviceAddr));
  EXPECT_EQ(kDeviceAddr, observer.last_connection_state_address());
  EXPECT_TRUE(observer.last_device_connected_state());

  // Disconnected
  fake_hal_iface_->NotifyAclStateChangedCallback(
      BT_STATUS_SUCCESS, hal_addr, BT_ACL_STATE_DISCONNECTED);
  EXPECT_FALSE(adapter_->IsDeviceConnected(kDeviceAddr));
  EXPECT_EQ(kDeviceAddr, observer.last_connection_state_address());
  EXPECT_FALSE(observer.last_device_connected_state());
}

}  // namespace
}  // namespace bluetooth
