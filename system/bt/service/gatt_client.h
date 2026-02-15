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

#include <mutex>
#include <unordered_map>

#include <base/macros.h>

#include "service/bluetooth_instance.h"
#include "service/common/bluetooth/uuid.h"
#include "service/hal/bluetooth_gatt_interface.h"

namespace bluetooth {

// A GattClient instance represents an application's handle to perform GATT
// client-role operations. Instances cannot be created directly and should be
// obtained through the factory.
class GattClient : public BluetoothInstance {
 public:
  ~GattClient() override;

  // BluetoothClientInstace overrides:
  const UUID& GetAppIdentifier() const override;
  int GetInstanceId() const override;

  bool RefreshDevice(std::string address);
  bool SetCharacteristicNotification(std::string address, int handle, bool enable);

 private:
  friend class GattClientFactory;

  // Constructor shouldn't be called directly as instances are meant to be
  // obtained from the factory.
  GattClient(const UUID& uuid, int client_id);

  // See getters above for documentation.
  UUID app_identifier_;
  int client_id_;

  DISALLOW_COPY_AND_ASSIGN(GattClient);
};

// GattClientFactory is used to register and obtain a per-application GattClient
// instance. Users should call RegisterClient to obtain their own unique
// GattClient instance that has been registered with the Bluetooth stack.
class GattClientFactory : public BluetoothInstanceFactory,
                          private hal::BluetoothGattInterface::ClientObserver {
 public:
  // Don't construct/destruct directly except in tests. Instead, obtain a handle
  // from an Adapter instance.
  GattClientFactory();
  ~GattClientFactory() override;

  // BluetoothInstanceFactory override:
  bool RegisterInstance(const UUID& uuid,
                        const RegisterCallback& callback) override;

 private:
  // hal::BluetoothGattInterface::ClientObserver override:
  void RegisterClientCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int client_id,
      const bt_uuid_t& app_uuid) override;

  // Map of pending calls to register.
  std::mutex pending_calls_lock_;
  std::unordered_map<UUID, RegisterCallback> pending_calls_;

  DISALLOW_COPY_AND_ASSIGN(GattClientFactory);
};

}  // namespace bluetooth
