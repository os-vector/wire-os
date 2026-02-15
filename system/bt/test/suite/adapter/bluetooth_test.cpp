/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "adapter/bluetooth_test.h"

#include <mutex>

extern "C" {
#include "btcore/include/property.h"
}

namespace {

// Mutex lock used by callbacks to protect |callback_semaphores_| from
// racey behaviour caused when Wait and Notify are called at the same time
std::mutex callback_lock;

}  // namespace

namespace bttest {

void BluetoothTest::SetUp() {
  bt_interface_ = nullptr;
  state_ = BT_STATE_OFF;
  properties_changed_count_ = 0;
  last_changed_properties_ = nullptr;
  discovery_state_ = BT_DISCOVERY_STOPPED;
  acl_state_ = BT_ACL_STATE_DISCONNECTED;
  bond_state_ = BT_BOND_STATE_NONE;

  adapter_properties_callback_sem_ = semaphore_new(0);
  adapter_state_changed_callback_sem_ = semaphore_new(0);
  discovery_state_changed_callback_sem_ = semaphore_new(0);

  bluetooth::hal::BluetoothInterface::Initialize();
  ASSERT_TRUE(bluetooth::hal::BluetoothInterface::IsInitialized());
  auto bt_hal_interface = bluetooth::hal::BluetoothInterface::Get();
  bt_hal_interface->AddObserver(this);
  bt_interface_ = bt_hal_interface->GetHALInterface();
  ASSERT_NE(nullptr, bt_interface_) << "bt_interface is null.";
}

void BluetoothTest::TearDown() {
  semaphore_free(adapter_properties_callback_sem_);
  semaphore_free(adapter_state_changed_callback_sem_);
  semaphore_free(discovery_state_changed_callback_sem_);

  auto bt_hal_interface = bluetooth::hal::BluetoothInterface::Get();
  bt_hal_interface->RemoveObserver(this);
  bt_hal_interface->CleanUp();
  ASSERT_FALSE(bt_hal_interface->IsInitialized());
}

void BluetoothTest::ClearSemaphore(semaphore_t* sem) {
  while (semaphore_try_wait(sem));
}

const bt_interface_t* BluetoothTest::bt_interface() {
  return bt_interface_;
}

bt_state_t BluetoothTest::GetState() {
  return state_;
}

int BluetoothTest::GetPropertiesChangedCount() {
  return properties_changed_count_;
}

bt_property_t* BluetoothTest::GetProperty(bt_property_type_t type) {
  for (int i = 0; i < properties_changed_count_; ++i) {
    if (last_changed_properties_[i].type == type) {
      return &last_changed_properties_[i];
    }
  }
  return nullptr;
}

bt_discovery_state_t BluetoothTest::GetDiscoveryState() {
  return discovery_state_;
}

bt_acl_state_t BluetoothTest::GetAclState() {
  return acl_state_;
}

// Returns the device bond state.
bt_bond_state_t BluetoothTest::GetBondState() {
  return bond_state_;
}

// callback
void BluetoothTest::AdapterStateChangedCallback(bt_state_t new_state) {
  state_ = new_state;
  semaphore_post(adapter_state_changed_callback_sem_);
}

// callback
void BluetoothTest::AdapterPropertiesCallback(
    bt_status_t status,
    int num_properties,
    bt_property_t* new_properties) {
  property_free_array(last_changed_properties_, properties_changed_count_);
  last_changed_properties_ = property_copy_array(new_properties, num_properties);
  properties_changed_count_ = num_properties;
  semaphore_post(adapter_properties_callback_sem_);
}

// callback
void BluetoothTest::DiscoveryStateChangedCallback(bt_discovery_state_t state) {
  discovery_state_ = state;
  semaphore_post(discovery_state_changed_callback_sem_);
}

} // bttest
