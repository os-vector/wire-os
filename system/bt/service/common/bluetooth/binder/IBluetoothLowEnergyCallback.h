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

#pragma once

#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

#include <bluetooth/advertise_data.h>
#include <bluetooth/advertise_settings.h>
#include <bluetooth/scan_result.h>

#include <hardware/bluetooth.h>
#include <hardware/bt_gatt_client.h>

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for receiving callbacks related
// to Bluetooth Low Energy operations.
// TODO(armansito): This class was written based on a new design doc proposal.
// We need to add an AIDL for this to the framework code.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
/* oneway */ class IBluetoothLowEnergyCallback : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(BluetoothLowEnergyCallback);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    ON_CLIENT_REGISTERED_TRANSACTION = android::IBinder::FIRST_CALL_TRANSACTION,
    ON_CONNECTION_STATE_TRANSACTION,
    ON_MTU_CHANGED_TRANSACTION,
    ON_SEARCH_COMPLETE_TRANSACTION,
    ON_SCAN_RESULT_TRANSACTION,
    ON_BATCH_SCAN_RESULTS_TRANSACTION,
    ON_READ_REMOTE_RSSI_TRANSACTION,
    ON_MULTI_ADVERTISE_CALLBACK_TRANSACTION,
    ON_SCAN_MANAGER_ERROR_CALLBACK_TRANSACTION,
    ON_CONFIGURE_ATT_MTU_TRANSACTION,
    ON_ATT_MTU_CHANGED_TRANSACTION,
    ON_FOUND_OR_LOST_TRANSACTION,
    ON_GATT_DB_UPDATED_TRANSACTION,
    ON_CHARACTERISTIC_READ_TRANSACTION,
    ON_CHARACTERISTIC_WRITE_TRANSACTION,
    ON_DESCRIPTOR_WRITE_TRANSACTION,
    ON_REGISTER_FOR_NOTIFICATION_CALLBACK_TRANSACTION,
    ON_NOTIFY_TRANSACTION,
  };

  virtual void OnClientRegistered(int status, int client_if) = 0;
  virtual void OnConnectionState(int status, int client_id, const char* address,
                                 bool connected) = 0;
  virtual void OnMtuChanged(int status, const char* address, int mtu) = 0;
  virtual void OnServicesDiscovered(int status, const char* address) = 0;
  virtual void OnGattDbUpdated(const char* address, btgatt_db_element_t* db, int size) = 0;
  virtual void OnCharacteristicRead(const char* address, int status, btgatt_read_params_t* p_data) = 0;
  virtual void OnCharacteristicWrite(const char* address, int status, uint16_t handle) = 0;
  virtual void OnDescriptorWrite(const char* address, int status, uint16_t handle) = 0;
  virtual void OnCharacteristicNotificationRegistration(const char* address, int registered,
                                                        int status, uint16_t handle) = 0;
  virtual void OnCharacteristicChanged(const char* address, btgatt_notify_params_t* notification) = 0;
  virtual void OnScanResult(const bluetooth::ScanResult& scan_result) = 0;
  virtual void OnMultiAdvertiseCallback(
      int status, bool is_start,
      const bluetooth::AdvertiseSettings& settings) = 0;

  // TODO(armansito): Complete the API definition.

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetoothLowEnergyCallback);
};

// The Binder server interface to allback. A class that
// implements IBluetoothLowEnergyCallback must inherit from this class.
class BnBluetoothLowEnergyCallback
    : public android::BnInterface<IBluetoothLowEnergyCallback> {
 public:
  BnBluetoothLowEnergyCallback() = default;
  virtual ~BnBluetoothLowEnergyCallback() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);

  DISALLOW_COPY_AND_ASSIGN(BnBluetoothLowEnergyCallback);
};

// The Binder client interface to IBluetoothLowEnergyCallback.
class BpBluetoothLowEnergyCallback
    : public android::BpInterface<IBluetoothLowEnergyCallback> {
 public:
  BpBluetoothLowEnergyCallback(const android::sp<android::IBinder>& impl);
  virtual ~BpBluetoothLowEnergyCallback() = default;

  // IBluetoothLowEnergyCallback overrides:
  void OnClientRegistered(int status, int client_if) override;
  void OnConnectionState(int status, int client_id, const char* address,
                         bool connected) override;
  void OnMtuChanged(int status, const char* address, int mtu) override;
  void OnServicesDiscovered(int status, const char* address) override;
  void OnGattDbUpdated(const char* address, btgatt_db_element_t* db, int size) override;
  void OnCharacteristicRead(const char* address, int status, btgatt_read_params_t* p_data) override;
  void OnCharacteristicWrite(const char* address, int status, uint16_t handle) override;
  void OnDescriptorWrite(const char* address, int status, uint16_t handle) override;
  void OnCharacteristicNotificationRegistration(const char* address, int registered,
                                                int status, uint16_t handle) override;
  void OnCharacteristicChanged(const char* address, btgatt_notify_params_t* notification) override;
  void OnScanResult(const bluetooth::ScanResult& scan_result) override;
  void OnMultiAdvertiseCallback(
      int status, bool is_start,
      const bluetooth::AdvertiseSettings& settings) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetoothLowEnergyCallback);
};

}  // namespace binder
}  // namespace ipc
