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

#include <memory>
#include <unordered_map>

#include <base/macros.h>

#include "service/bluetooth_instance.h"
#include "service/common/bluetooth/uuid.h"
#include "service/ipc/binder/remote_callback_map.h"

namespace ipc {
namespace binder {

// InterfaceWithInstancesBase provides a common base class for Binder interface
// servers that involve instance callback Binders registered with an integer
// instance ID over an asynchronous lower-level stack API. This class abstracts
// away the common procedures of managing pending callbacks, listening to death
// notifications, and maintaining multiple internal maps in one common base
// class.
// TODO: add code example here.
class InterfaceWithInstancesBase
    : public RemoteCallbackMap<int, android::IInterface>::Delegate,
      virtual public android::RefBase {
 public:
  InterfaceWithInstancesBase() = default;
  ~InterfaceWithInstancesBase() override = default;

 protected:
  // The initial entry point for registering a instance. Invoke this from the
  // registration API to add a instance/UUID pair to the pending list and set up
  // the generic asynchronous callback handler and initiate the process with the
  // given |factory| instance. Returns false, if there were any errors that
  // could be synchronously reported.
  bool RegisterInstanceBase(const android::sp<IInterface>& callback,
                            bluetooth::BluetoothInstanceFactory* factory);

  // Unregister the instance with the given ID, if it was registered before.
  void UnregisterInstanceBase(int instance_id);

  // Unregisters all registered instances.
  void UnregisterAllBase();

  // Returns a handle to the lock used to synchronize access to the internal
  // data structures. Subclasses should acquire this before accessing the maps.
  std::mutex* maps_lock() { return &maps_lock_; }

  // Returns the callback interface binder that is assigned to the given
  // instance ID |instance_id|. The returned pointer will contain NULL if an
  // entry for the given ID cannot be found.
  android::sp<IInterface> GetCallback(int instance_id);

  // Returns the instance instance that is assigned to the given instance ID
  // |instance_id|. The returned pointer will contain NULL if an entry for the
  // given ID cannot be found.
  std::shared_ptr<bluetooth::BluetoothInstance> GetInstance(
      int instance_id);

 private:
  // Base implementation of the register callback.
  void OnRegisterInstance(
      bluetooth::BLEStatus status,
      const bluetooth::UUID& uuid,
      std::unique_ptr<bluetooth::BluetoothInstance> instance);

  // Called when the callback registration has completed. |instance| is owned by
  // the base class and should not be deleted by the implementation. If the
  // operation failed, nullptr will be passed for |instance|.
  virtual void OnRegisterInstanceImpl(
      bluetooth::BLEStatus status,
      android::sp<IInterface> callback,
      bluetooth::BluetoothInstance* instance) = 0;

  // RemoteCallbackMap<int, IBluetoothLowEnergyCallback>::Delegate override:
  void OnRemoteCallbackRemoved(const int& key) override;

  // Instances that are pending registration. Once their registration is
  // complete, the entry will be removed from this map.
  RemoteCallbackMap<bluetooth::UUID, android::IInterface> pending_callbacks_;

  // We keep two maps here: one from instance_id IDs to callback Binders and one
  // from instance_id IDs to the BluetoothInstance structures themselves.
  std::mutex maps_lock_;  // Needed for |id_to_instance_|.
  RemoteCallbackMap<int, IInterface> id_to_cb_;
  std::unordered_map<int, std::shared_ptr<bluetooth::BluetoothInstance>>
      id_to_instance_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceWithInstancesBase);
};

}  // namespace binder
}  // namespace ipc
