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

#include <bluetooth/adapter_state.h>

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for receiving adapter state
// updates from the Bluetooth service. This class was written based on the
// corresponding AIDL file at
// /frameworks/base/core/java/android/bluetooth/IBluetoothCallback.aidl.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
class IBluetoothCallback : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(BluetoothCallback);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    ON_BLUETOOTH_STATE_CHANGE_TRANSACTION =
        android::IBinder::FIRST_CALL_TRANSACTION,
  };

  virtual void OnBluetoothStateChange(
      bluetooth::AdapterState prev_state,
      bluetooth::AdapterState new_state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetoothCallback);
};

// TODO(armansito): Implement notification for when the process dies.

// The Binder server interface to IBluetoothCallback. A class that implements
// IBluetoothCallback must inherit from this class.
class BnBluetoothCallback : public android::BnInterface<IBluetoothCallback> {
 public:
  BnBluetoothCallback() = default;
  virtual ~BnBluetoothCallback() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);
};

// The Binder client interface to IBluetoothCallback.
class BpBluetoothCallback : public android::BpInterface<IBluetoothCallback> {
 public:
  BpBluetoothCallback(const android::sp<android::IBinder>& impl);
  virtual ~BpBluetoothCallback() = default;

  // IBluetoothCallback override:
  void OnBluetoothStateChange(bluetooth::AdapterState prev_state,
                              bluetooth::AdapterState new_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetoothCallback);
};

}  // namespace binder
}  // namespace ipc
