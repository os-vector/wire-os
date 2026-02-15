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

#include <cstdint>
#include <string>
#include <vector>

#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

#include <bluetooth/gatt_identifier.h>

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for receiving callbacks related
// to Bluetooth GATT server-role operations.
// TODO(armansito): This class was written based on a new design doc proposal.
// We need to add an AIDL for this to the framework code.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
/* oneway */ class IBluetoothGattServerCallback : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(BluetoothGattServerCallback);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    ON_SERVER_REGISTERED_TRANSACTION = android::IBinder::FIRST_CALL_TRANSACTION,
    ON_SERVICE_ADDED_TRANSACTION,
    ON_CHARACTERISTIC_READ_REQUEST_TRANSACTION,
    ON_DESCRIPTOR_READ_REQUEST_TRANSACTION,
    ON_CHARACTERISTIC_WRITE_REQUEST_TRANSACTION,
    ON_DESCRIPTOR_WRITE_REQUEST_TRANSACTION,
    ON_EXECUTE_WRITE_REQUEST_TRANSACTION,
    ON_NOTIFICATION_SENT_TRANSACTION,
  };

  virtual void OnServerRegistered(int status, int server_if) = 0;

  virtual void OnServiceAdded(
      int status, const bluetooth::GattIdentifier& service_id) = 0;

  virtual void OnCharacteristicReadRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_long,
      const bluetooth::GattIdentifier& characteristic_id) = 0;

  virtual void OnDescriptorReadRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_long,
      const bluetooth::GattIdentifier& descriptor_id) = 0;

  virtual void OnCharacteristicWriteRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_prepare_write, bool need_response,
      const std::vector<uint8_t>& value,
      const bluetooth::GattIdentifier& characteristic_id) = 0;

  virtual void OnDescriptorWriteRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_prepare_write, bool need_response,
      const std::vector<uint8_t>& value,
      const bluetooth::GattIdentifier& descriptor_id) = 0;

  virtual void OnExecuteWriteRequest(
      const std::string& device_address,
      int request_id, bool is_execute) = 0;

  virtual void OnNotificationSent(const std::string& device_address,
                                  int status) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetoothGattServerCallback);
};

// The Binder server interface to IBluetoothGattServerCallback. A class that
// implements IBluetoothGattServerCallback must inherit from this class.
class BnBluetoothGattServerCallback
    : public android::BnInterface<IBluetoothGattServerCallback> {
 public:
  BnBluetoothGattServerCallback() = default;
  virtual ~BnBluetoothGattServerCallback() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);

  DISALLOW_COPY_AND_ASSIGN(BnBluetoothGattServerCallback);
};

// The Binder client interface to IBluetoothGattServerCallback.
class BpBluetoothGattServerCallback
    : public android::BpInterface<IBluetoothGattServerCallback> {
 public:
  explicit BpBluetoothGattServerCallback(
      const android::sp<android::IBinder>& impl);
  virtual ~BpBluetoothGattServerCallback() = default;

  // IBluetoothGattServerCallback overrides:
  void OnServerRegistered(int status, int server_if) override;
  void OnServiceAdded(int status,
                      const bluetooth::GattIdentifier& service_id) override;
  void OnCharacteristicReadRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_long,
      const bluetooth::GattIdentifier& characteristic_id) override;
  void OnDescriptorReadRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_long,
      const bluetooth::GattIdentifier& descriptor_id) override;
  void OnCharacteristicWriteRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_prepare_write, bool need_response,
      const std::vector<uint8_t>& value,
      const bluetooth::GattIdentifier& characteristic_id) override;
  void OnDescriptorWriteRequest(
      const std::string& device_address,
      int request_id, int offset, bool is_prepare_write, bool need_response,
      const std::vector<uint8_t>& value,
      const bluetooth::GattIdentifier& descriptor_id) override;
  void OnExecuteWriteRequest(
      const std::string& device_address,
      int request_id, bool is_execute) override;
  void OnNotificationSent(const std::string& device_address,
                          int status) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetoothGattServerCallback);
};

}  // namespace binder
}  // namespace ipc
