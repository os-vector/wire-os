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
#include "gatt/gatt_test.h"

extern "C" {
#include "btcore/include/bdaddr.h"
}

namespace bttest {

void GattTest::SetUp() {
  gatt_client_interface_ = nullptr;
  gatt_server_interface_ = nullptr;

  client_interface_id_ = 0;
  server_interface_id_ = 0;
  service_handle_ = 0;
  characteristic_handle_ = 0;
  descriptor_handle_ = 0;
  status_ = 0;

  BluetoothTest::SetUp();
  ASSERT_EQ(bt_interface()->enable(false), BT_STATUS_SUCCESS);
  semaphore_wait(adapter_state_changed_callback_sem_);
  EXPECT_TRUE(GetState() == BT_STATE_ON);

  register_client_callback_sem_ = semaphore_new(0);
  scan_result_callback_sem_ = semaphore_new(0);
  listen_callback_sem_ = semaphore_new(0);

  register_server_callback_sem_ = semaphore_new(0);
  service_added_callback_sem_ = semaphore_new(0);
  characteristic_added_callback_sem_ = semaphore_new(0);
  descriptor_added_callback_sem_ = semaphore_new(0);
  service_started_callback_sem_ = semaphore_new(0);
  service_stopped_callback_sem_ = semaphore_new(0);
  service_deleted_callback_sem_ = semaphore_new(0);

  bluetooth::hal::BluetoothGattInterface::Initialize();
  ASSERT_TRUE(bluetooth::hal::BluetoothGattInterface::IsInitialized());
  auto gatt_interface = bluetooth::hal::BluetoothGattInterface::Get();
  gatt_interface->AddClientObserver(this);
  gatt_interface->AddServerObserver(this);

  gatt_client_interface_ = gatt_interface->GetClientHALInterface();
  gatt_server_interface_ = gatt_interface->GetServerHALInterface();

  ASSERT_NE(nullptr, gatt_client_interface_);
  ASSERT_NE(nullptr, gatt_server_interface_);
}

void GattTest::TearDown() {
  gatt_client_interface_ = nullptr;
  gatt_server_interface_ = nullptr;

  semaphore_free(register_client_callback_sem_);
  semaphore_free(scan_result_callback_sem_);
  semaphore_free(listen_callback_sem_);

  semaphore_free(register_server_callback_sem_);
  semaphore_free(service_added_callback_sem_);
  semaphore_free(characteristic_added_callback_sem_);
  semaphore_free(descriptor_added_callback_sem_);
  semaphore_free(service_started_callback_sem_);
  semaphore_free(service_stopped_callback_sem_);
  semaphore_free(service_deleted_callback_sem_);

  bluetooth::hal::BluetoothGattInterface::CleanUp();

  ASSERT_EQ(bt_interface()->disable(), BT_STATUS_SUCCESS);
  semaphore_wait(adapter_state_changed_callback_sem_);
  BluetoothTest::TearDown();
}

const btgatt_client_interface_t* GattTest::gatt_client_interface() {
  return gatt_client_interface_;
}

const btgatt_server_interface_t* GattTest::gatt_server_interface() {
  return gatt_server_interface_;
}

void GattTest::RegisterClientCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int clientIf, const bt_uuid_t& app_uuid) {
  status_ = status;
  client_interface_id_ = clientIf;
  semaphore_post(register_client_callback_sem_);
}

void GattTest::ScanResultCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    const bt_bdaddr_t& bda, int rssi, uint8_t* adv_data) {
  semaphore_post(scan_result_callback_sem_);
}

void GattTest::ListenCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int client_if) {
  status_ = status;
  client_interface_id_ = client_if;
  semaphore_post(listen_callback_sem_);
}

// GATT server callbacks
void GattTest::RegisterServerCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int server_if, const bt_uuid_t& uuid) {
  status_ = status;
  server_interface_id_ = server_if;
  semaphore_post(register_server_callback_sem_);
}

void GattTest::ServiceAddedCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int server_if, const btgatt_srvc_id_t& srvc_id,
    int srvc_handle) {
  status_ = status;
  server_interface_id_ = server_if;
  service_handle_ = srvc_handle;
  semaphore_post(service_added_callback_sem_);
}

void GattTest::CharacteristicAddedCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int server_if, const bt_uuid_t& char_id,
    int srvc_handle, int char_handle) {
  status_ = status;
  server_interface_id_ = server_if;
  service_handle_ = srvc_handle;
  characteristic_handle_ = char_handle;
  semaphore_post(characteristic_added_callback_sem_);
}

void GattTest::DescriptorAddedCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int server_if, const bt_uuid_t& descr_id,
    int srvc_handle, int descr_handle) {
  status_ = status;
  server_interface_id_ = server_if;
  service_handle_ = srvc_handle;
  descriptor_handle_ = descr_handle;
  semaphore_post(descriptor_added_callback_sem_);
}

void GattTest::ServiceStartedCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int server_if, int srvc_handle) {
  status_ = status;
  server_interface_id_ = server_if;
  service_handle_ = srvc_handle;
  semaphore_post(service_started_callback_sem_);
}

void GattTest::ServiceStoppedCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int server_if, int srvc_handle) {
  status_ = status;
  server_interface_id_ = server_if;
  service_handle_ = srvc_handle;
  semaphore_post(service_stopped_callback_sem_);
}

void GattTest::ServiceDeletedCallback(
    bluetooth::hal::BluetoothGattInterface* /* unused */,
    int status, int server_if, int srvc_handle) {
  status_ = status;
  server_interface_id_ = server_if;
  service_handle_ = srvc_handle;
  semaphore_post(service_deleted_callback_sem_);
}

}  // bttest
