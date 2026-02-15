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

#include "service/ipc/binder/interface_with_instances_base.h"

#include <base/logging.h>

namespace ipc {
namespace binder {

bool InterfaceWithInstancesBase::RegisterInstanceBase(
    const android::sp<IInterface>& callback,
    bluetooth::BluetoothInstanceFactory* factory) {
  VLOG(2) << __func__;
  CHECK(factory);

  if (!callback.get()) {
    LOG(ERROR) << "Cannot register a NULL callback";
    return false;
  }

  // Store the callback in the pending list. It will get removed later when the
  // stack notifies us asynchronously.
  bluetooth::UUID app_uuid = bluetooth::UUID::GetRandom();
  if (!pending_callbacks_.Register(app_uuid, callback)) {
    LOG(ERROR) << "Failed to store |callback| to map";
    return false;
  }

  // Create a weak pointer and pass that to the callback to prevent an invalid
  // access later. Since this object is managed using Android's StrongPointer
  // (sp) we are using a wp here rather than std::weak_ptr.
  android::wp<InterfaceWithInstancesBase> weak_ptr_to_this(this);

  bluetooth::BluetoothInstanceFactory::RegisterCallback cb =
      [weak_ptr_to_this](
          bluetooth::BLEStatus status,
          const bluetooth::UUID& in_uuid,
          std::unique_ptr<bluetooth::BluetoothInstance> instance) {
        // If the weak pointer was invalidated then there is nothing we can do.
        android::sp<InterfaceWithInstancesBase> strong_ptr_to_this =
            weak_ptr_to_this.promote();
        if (!strong_ptr_to_this.get()) {
          VLOG(2) << "InterfaceWithInstancesBase was deleted while instance was"
                  << " being registered";
          return;
        }

        strong_ptr_to_this->OnRegisterInstance(
            status, in_uuid, std::move(instance));
      };

  if (factory->RegisterInstance(app_uuid, cb))
    return true;

  LOG(ERROR) << "Failed to register instance";
  pending_callbacks_.Remove(app_uuid);

  return false;
}

void InterfaceWithInstancesBase::UnregisterInstanceBase(int instance_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(maps_lock_);

  id_to_cb_.Remove(instance_id);
  id_to_instance_.erase(instance_id);
}

void InterfaceWithInstancesBase::UnregisterAllBase() {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(maps_lock_);

  id_to_cb_.Clear();
  id_to_instance_.clear();
}

android::sp<IInterface> InterfaceWithInstancesBase::GetCallback(
    int instance_id) {
  return id_to_cb_.Get(instance_id);
}

std::shared_ptr<bluetooth::BluetoothInstance>
InterfaceWithInstancesBase::GetInstance(int instance_id) {
  auto iter = id_to_instance_.find(instance_id);
  if (iter == id_to_instance_.end())
    return std::shared_ptr<bluetooth::BluetoothInstance>();
  return iter->second;
}

void InterfaceWithInstancesBase::OnRegisterInstance(
    bluetooth::BLEStatus status,
    const bluetooth::UUID& uuid,
    std::unique_ptr<bluetooth::BluetoothInstance> instance) {
  VLOG(2) << __func__ << " - status: " << status;

  // Simply remove the callback from |pending_callbacks_| as it no longer
  // belongs in there.
  sp<IInterface> callback = pending_callbacks_.Remove(uuid);

  // |callback| might be NULL if it was removed from the pending list, e.g. the
  // remote process that owns the callback died.
  if (!callback.get()) {
    VLOG(1) << "Callback was removed before the call to \"RegisterInstance\" "
            << "returned; unregistering instance";
    return;
  }

  if (status != bluetooth::BLE_STATUS_SUCCESS) {
    // The call wasn't successful. Notify the implementation and return.
    LOG(ERROR) << "Failed to register instance: " << status;
    OnRegisterInstanceImpl(status, callback, nullptr);
    return;
  }

  std::lock_guard<std::mutex> lock(maps_lock_);
  int instance_id = instance->GetInstanceId();
  CHECK(instance_id);
  if (!id_to_cb_.Register(instance_id, callback, this)) {
    LOG(ERROR) << "Failed to store callback";
    OnRegisterInstanceImpl(bluetooth::BLE_STATUS_FAILURE, callback, nullptr);
    return;
  }

  VLOG(1) << "Registered BluetoothInstance - ID: " << instance_id;

  auto shared_instance =
      std::shared_ptr<bluetooth::BluetoothInstance>(instance.release());
  id_to_instance_[instance_id] = shared_instance;

  OnRegisterInstanceImpl(status, callback, shared_instance.get());
}

void InterfaceWithInstancesBase::OnRemoteCallbackRemoved(const int& key) {
  VLOG(2) << __func__ << " instance_id: " << key;
  std::lock_guard<std::mutex> lock(maps_lock_);

  // No need to remove from the callback map as the entry should be already
  // removed when this callback gets called.
  id_to_instance_.erase(key);
}

}  // namespace binder
}  // namespace ipc
