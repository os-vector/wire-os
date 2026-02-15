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
#include <memory>
#include <vector>

#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

#include <bluetooth/binder/IBluetoothGattServerCallback.h>
#include <bluetooth/gatt_identifier.h>

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for interacting with Bluetooth
// GATT server-role features.
// TODO(armansito): This class was written based on a new design doc proposal.
// We need to add an AIDL for this to the framework code.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
class IBluetoothGattServer : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(BluetoothGattServer);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    REGISTER_SERVER_TRANSACTION = android::IBinder::FIRST_CALL_TRANSACTION,
    UNREGISTER_SERVER_TRANSACTION,
    UNREGISTER_ALL_TRANSACTION,
    BEGIN_SERVICE_DECLARATION_TRANSACTION,
    ADD_INCLUDED_SERVICE_TRANSACTION,
    ADD_CHARACTERISTIC_TRANSACTION,
    ADD_DESCRIPTOR_TRANSACTION,
    END_SERVICE_DECLARATION_TRANSACTION,
    REMOVE_SERVICE_TRANSACTION,
    CLEAR_SERVICES_TRANSACTION,
    SEND_RESPONSE_TRANSACTION,
    SEND_NOTIFICATION_TRANSACTION,
  };

  virtual bool RegisterServer(
      const android::sp<IBluetoothGattServerCallback>& callback) = 0;
  virtual void UnregisterServer(int server_if) = 0;
  virtual void UnregisterAll() = 0;

  virtual bool BeginServiceDeclaration(
      int server_if, bool is_primary, const bluetooth::UUID& uuid,
      std::unique_ptr<bluetooth::GattIdentifier>* out_id) = 0;
  virtual bool AddCharacteristic(
      int server_if, const bluetooth::UUID& uuid,
      int properties, int permissions,
      std::unique_ptr<bluetooth::GattIdentifier>* out_id) = 0;
  virtual bool AddDescriptor(
      int server_if, const bluetooth::UUID& uuid, int permissions,
      std::unique_ptr<bluetooth::GattIdentifier>* out_id) = 0;
  virtual bool EndServiceDeclaration(int server_if) = 0;

  virtual bool SendResponse(
      int server_if,
      const std::string& device_address,
      int request_id,
      int status, int offset,
      const std::vector<uint8_t>& value) = 0;

  virtual bool SendNotification(
      int server_if,
      const std::string& device_address,
      const bluetooth::GattIdentifier& characteristic_id,
      bool confirm,
      const std::vector<uint8_t>& value) = 0;

  // TODO(armansito): Complete the API definition.

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetoothGattServer);
};

// The Binder server interface to IBluetoothGattServer. A class that implements
// IBluetoothGattServer must inherit from this class.
class BnBluetoothGattServer
    : public android::BnInterface<IBluetoothGattServer> {
 public:
  BnBluetoothGattServer() = default;
  virtual ~BnBluetoothGattServer() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);

  DISALLOW_COPY_AND_ASSIGN(BnBluetoothGattServer);
};

// The Binder client interface to IBluetoothGattServer.
class BpBluetoothGattServer
    : public android::BpInterface<IBluetoothGattServer> {
 public:
  explicit BpBluetoothGattServer(const android::sp<android::IBinder>& impl);
  virtual ~BpBluetoothGattServer() = default;

  // IBluetoothGattServer overrides:
  bool RegisterServer(
      const android::sp<IBluetoothGattServerCallback>& callback) override;
  void UnregisterServer(int server_if) override;
  void UnregisterAll() override;
  bool BeginServiceDeclaration(
      int server_if, bool is_primary, const bluetooth::UUID& uuid,
      std::unique_ptr<bluetooth::GattIdentifier>* out_id) override;
  bool AddCharacteristic(
      int server_if, const bluetooth::UUID& uuid,
      int properties, int permissions,
      std::unique_ptr<bluetooth::GattIdentifier>* out_id) override;
  bool AddDescriptor(
      int server_if, const bluetooth::UUID& uuid, int permissions,
      std::unique_ptr<bluetooth::GattIdentifier>* out_id) override;
  bool EndServiceDeclaration(int server_if) override;
  bool SendResponse(
      int server_if,
      const std::string& device_address,
      int request_id,
      int status, int offset,
      const std::vector<uint8_t>& value) override;
  bool SendNotification(
      int server_if,
      const std::string& device_address,
      const bluetooth::GattIdentifier& characteristic_id,
      bool confirm,
      const std::vector<uint8_t>& value) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetoothGattServer);
};

}  // namespace binder
}  // namespace ipc
