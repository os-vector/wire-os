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
#include "service/hal/bluetooth_gatt_interface.h"

namespace bttest {

// This class represents the Bluetooth GATT testing framework and provides
// helpers and callbacks for GUnit to use for testing gatt.
class GattTest : public BluetoothTest,
                 public bluetooth::hal::BluetoothGattInterface::ClientObserver,
                 public bluetooth::hal::BluetoothGattInterface::ServerObserver {
 protected:
  GattTest() = default;
  virtual ~GattTest() = default;

  // Gets the gatt_client_interface
  const btgatt_client_interface_t* gatt_client_interface();

  // Gets the gatt_server_interface
  const btgatt_server_interface_t* gatt_server_interface();

  // Getters for variables that track GATT-related state
  int client_interface_id() const { return client_interface_id_; }
  int server_interface_id() const { return server_interface_id_; }
  int service_handle() const { return service_handle_; }
  int characteristic_handle() const { return characteristic_handle_; }
  int descriptor_handle() const { return descriptor_handle_; }
  int status() const { return status_; }

  // SetUp initializes the Bluetooth interfaces and the GATT Interface as well
  // as registers the callbacks and initializes the semaphores before every test
  virtual void SetUp();

  // TearDown cleans up the Bluetooth and GATT interfaces and destroys the
  // callback semaphores at the end of every test
  virtual void TearDown();

  // bluetooth::hal::BluetoothGattInterface::ClientObserver overrides
  void RegisterClientCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int clientIf, const bt_uuid_t& app_uuid) override;
  void ScanResultCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      const bt_bdaddr_t& bda, int rssi, uint8_t* adv_data) override;
  void ListenCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int client_if) override;

  // bluetooth::hal::BluetoothGattInterface::ServerObserver overrides
  void RegisterServerCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int server_if, const bt_uuid_t& uuid) override;
  void ServiceAddedCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int server_if, const btgatt_srvc_id_t& srvc_id,
      int srvc_handle) override;
  void CharacteristicAddedCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int server_if, const bt_uuid_t& char_id,
      int srvc_handle, int char_handle) override;
  void DescriptorAddedCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int server_if, const bt_uuid_t& descr_id,
      int srvc_handle, int descr_handle) override;
  void ServiceStartedCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int server_if, int srvc_handle) override;
  void ServiceStoppedCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int server_if, int srvc_handle) override;
  void ServiceDeletedCallback(
      bluetooth::hal::BluetoothGattInterface* /* unused */,
      int status, int server_if, int srvc_handle) override;

  // Semaphores used to wait for specific callback execution. Each callback
  // has its own semaphore associated with it
  semaphore_t* register_client_callback_sem_;
  semaphore_t* scan_result_callback_sem_;
  semaphore_t* listen_callback_sem_;

  semaphore_t* register_server_callback_sem_;
  semaphore_t* service_added_callback_sem_;
  semaphore_t* characteristic_added_callback_sem_;
  semaphore_t* descriptor_added_callback_sem_;
  semaphore_t* service_started_callback_sem_;
  semaphore_t* service_stopped_callback_sem_;
  semaphore_t* service_deleted_callback_sem_;

 private:
  // The gatt_client_interface that all the tests use to interact with the HAL
  const btgatt_client_interface_t* gatt_client_interface_;

  // The gatt_server_interface that all the tests use to interact with the HAL
  const btgatt_server_interface_t* gatt_server_interface_;

  // No mutex needed for these as the semaphores should ensure
  // synchronous access

  // An ID that is used as a handle for each gatt client.
  int client_interface_id_;

  // An ID that is used as a handle for each gatt server.
  int server_interface_id_;

  // A handle to the last used service.
  int service_handle_;

  // A handle to the last characteristic added.
  int characteristic_handle_;

  // A handle to the last descriptor added.
  int descriptor_handle_;

  // The status of the last callback. Is BT_STATUS_SUCCESS if no issues.
  int status_;

  DISALLOW_COPY_AND_ASSIGN(GattTest);
};

}  // bttest
