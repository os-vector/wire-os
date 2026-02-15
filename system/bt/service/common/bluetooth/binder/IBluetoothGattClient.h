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

#include <string>

#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

#include <bluetooth/binder/IBluetoothGattClientCallback.h>
#include <bluetooth/gatt_identifier.h>

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for interacting with Bluetooth
// GATT client-role features.
// TODO(armansito): This class was written based on a new design doc proposal.
// We need to add an AIDL for this to the framework code.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
class IBluetoothGattClient : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(BluetoothGattClient);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    REGISTER_CLIENT_TRANSACTION = android::IBinder::FIRST_CALL_TRANSACTION,
    UNREGISTER_CLIENT_TRANSACTION,
    UNREGISTER_ALL_TRANSACTION,
    REFRESH_DEVICE_TRANSACTION,
    READ_DESCRIPTOR_TRANSACTION,
    REGISTER_FOR_NOTIFICATIONS_TRANSACTION,
    UNREGISTER_FOR_NOTIFICATIONS_TRANSACTION,
    BEGIN_RELIABLE_WRITE_TRANSACTION,
    END_RELIABLE_WRITE_TRANSACTION,
  };

  virtual bool RegisterClient(
      const android::sp<IBluetoothGattClientCallback>& callback) = 0;
  virtual void UnregisterClient(int client_id) = 0;
  virtual void UnregisterAll() = 0;
  virtual bool RefreshDevice(int client_id, const char* address) = 0;
  virtual bool SetCharacteristicNotification(int client_id, const char* address,
                                             int handle, bool enable) = 0;

  // TODO(armansito): Complete interface definition.

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetoothGattClient);
};

// The Binder server interface to IBluetoothGattClient. A class that implements
// IBluetoothGattClient must inherit from this class.
class BnBluetoothGattClient
    : public android::BnInterface<IBluetoothGattClient> {
 public:
  BnBluetoothGattClient() = default;
  virtual ~BnBluetoothGattClient() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);

  DISALLOW_COPY_AND_ASSIGN(BnBluetoothGattClient);
};

// The Binder client interface to IBluetoothGattClient.
class BpBluetoothGattClient
    : public android::BpInterface<IBluetoothGattClient> {
 public:
  explicit BpBluetoothGattClient(const android::sp<android::IBinder>& impl);
  virtual ~BpBluetoothGattClient() = default;

  // IBluetoothGattClient overrides:
  bool RegisterClient(
      const android::sp<IBluetoothGattClientCallback>& callback) override;
  void UnregisterClient(int client_id) override;
  void UnregisterAll() override;
  bool RefreshDevice(int client_id, const char* address) override;
  bool SetCharacteristicNotification(int client_id, const char* address,
                                     int handle, bool enable) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetoothGattClient);
};

}  // namespace binder
}  // namespace ipc
