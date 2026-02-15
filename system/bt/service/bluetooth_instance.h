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

#include <functional>
#include <memory>

#include <base/macros.h>

#include "service/common/bluetooth/low_energy_constants.h"
#include "service/common/bluetooth/uuid.h"

namespace bluetooth {

// A BluetoothInstance represents an application's handle to an instance
// that is registered with the underlying Bluetooth stack using a UUID and has a
// stack-assigned integer "instance_id" ID associated with it.
class BluetoothInstance {
 public:
  virtual ~BluetoothInstance() = default;

  // Returns the app-specific unique ID used while registering this instance.
  virtual const UUID& GetAppIdentifier() const = 0;

  // Returns the HAL "interface ID" assigned to this instance by the stack.
  virtual int GetInstanceId() const = 0;

 protected:
  // Constructor shouldn't be called directly as instances are meant to be
  // obtained from the factory.
  BluetoothInstance() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothInstance);
};

// A BluetoothInstanceFactory provides a common interface for factory
// classes that handle asynchronously registering a per-application instance of
// a BluetoothInstance with the underlying stack.
class BluetoothInstanceFactory {
 public:
  BluetoothInstanceFactory() = default;
  virtual ~BluetoothInstanceFactory() = default;

  // Callback invoked as a result of a call to RegisterInstance.
  using RegisterCallback = std::function<void(
      BLEStatus status, const UUID& app_uuid,
      std::unique_ptr<BluetoothInstance> instance)>;

  // Registers an instance for the given unique identifier |app_uuid|.
  // On success, this asynchronously invokes |callback| with a unique pointer
  // to a BluetoothInstance whose ownership can be taken by the caller. In
  // the case of an error, the pointer will contain nullptr.
  virtual bool RegisterInstance(const UUID& app_uuid,
                                const RegisterCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothInstanceFactory);
};

}  // namespace bluetooth
