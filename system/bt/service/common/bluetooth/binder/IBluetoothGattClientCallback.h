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

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for receiving callbacks related
// to Bluetooth GATT client-role operations.
// TODO(armansito): This class was written based on a new design doc proposal.
// We need to add an AIDL for this to the framework code.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
/* oneway */ class IBluetoothGattClientCallback : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(BluetoothGattClientCallback);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    ON_CLIENT_REGISTERED_TRANSACTION = android::IBinder::FIRST_CALL_TRANSACTION,
    ON_GET_SERVICE_TRANSACTION,
    ON_GET_INCLUDED_SERVICE_TRANSACTION,
    ON_GET_CHARACTERISTIC_TRANSACTION,
    ON_GET_DESCRIPTOR_TRANSACTION,
    ON_EXECUTE_WRITE_TRANSACTION,
    ON_DESCRIPTOR_READ_TRANSACTION,
  };

  virtual void OnClientRegistered(int status, int client_id) = 0;

  // TODO(armansito): Complete interface definition.

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetoothGattClientCallback);
};

// The Binder server interface to IBluetoothGattClientCallback. A class that
// implements IBluetoothGattClientCallback must inherit from this class.
class BnBluetoothGattClientCallback
    : public android::BnInterface<IBluetoothGattClientCallback> {
 public:
  BnBluetoothGattClientCallback() = default;
  virtual ~BnBluetoothGattClientCallback() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);

  DISALLOW_COPY_AND_ASSIGN(BnBluetoothGattClientCallback);
};

// The Binder client interface to IBluetoothGattClientCallback.
class BpBluetoothGattClientCallback
    : public android::BpInterface<IBluetoothGattClientCallback> {
 public:
  explicit BpBluetoothGattClientCallback(
      const android::sp<android::IBinder>& impl);
  virtual ~BpBluetoothGattClientCallback() = default;

  // IBluetoothGattClientCallback overrides:
  void OnClientRegistered(int status, int server_if) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetoothGattClientCallback);
};

}  // namespace binder
}  // namespace ipc
