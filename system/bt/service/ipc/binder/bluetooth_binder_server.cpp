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

#include "service/ipc/binder/bluetooth_binder_server.h"

#include <base/logging.h>

#include "service/ipc/binder/bluetooth_gatt_client_binder_server.h"
#include "service/ipc/binder/bluetooth_gatt_server_binder_server.h"
#include "service/ipc/binder/bluetooth_low_energy_binder_server.h"

#include "service/hal/bluetooth_interface.h"

using android::sp;

namespace ipc {
namespace binder {

BluetoothBinderServer::BluetoothBinderServer(bluetooth::Adapter* adapter)
    : adapter_(adapter) {
  CHECK(adapter_);
  adapter_->AddObserver(this);
}

BluetoothBinderServer::~BluetoothBinderServer() {
  adapter_->RemoveObserver(this);
}

// binder::BnBluetooth overrides:
bool BluetoothBinderServer::IsEnabled() {
  VLOG(2) << __func__;
  return adapter_->IsEnabled();
}

int BluetoothBinderServer::GetState() {
  VLOG(2) << __func__;
  return adapter_->GetState();
}

bool BluetoothBinderServer::Enable(bool start_restricted) {
  VLOG(2) << __func__;
  return adapter_->Enable(start_restricted);
}

bool BluetoothBinderServer::EnableNoAutoConnect() {
  VLOG(2) << __func__;
  // TODO(armansito): Implement.
  return false;
}

bool BluetoothBinderServer::Disable() {
  VLOG(2) << __func__;
  return adapter_->Disable();
}

std::string BluetoothBinderServer::GetAddress() {
  VLOG(2) << __func__;
  return adapter_->GetAddress();
}

std::vector<bluetooth::UUID> BluetoothBinderServer::GetUUIDs() {
  VLOG(2) << __func__;
  // TODO(armansito): Implement.
  return std::vector<bluetooth::UUID>();
}

bool BluetoothBinderServer::SetName(const std::string& name) {
  VLOG(2) << __func__;
  return adapter_->SetName(name);
}

std::string BluetoothBinderServer::GetName() {
  VLOG(2) << __func__;
  return adapter_->GetName();
}

void BluetoothBinderServer::RegisterCallback(
    const sp<IBluetoothCallback>& callback) {
  VLOG(2) << __func__;
  if (!callback.get() ) {
    LOG(ERROR) << "RegisterCallback called with NULL binder. Ignoring.";
    return;
  }
  callbacks_.Register(callback);
}

void BluetoothBinderServer::UnregisterCallback(
    const sp<IBluetoothCallback>& callback) {
  VLOG(2) << __func__;
  if (!callback.get() ) {
    LOG(ERROR) << "UnregisterCallback called with NULL binder. Ignoring.";
    return;
  }
  callbacks_.Unregister(callback);
}

bool BluetoothBinderServer::IsMultiAdvertisementSupported() {
  VLOG(2) << __func__;
  return adapter_->IsMultiAdvertisementSupported();
}

sp<IBluetoothLowEnergy>
BluetoothBinderServer::GetLowEnergyInterface() {
  VLOG(2) << __func__;

  if (!adapter_->IsEnabled()) {
    LOG(ERROR) << "Cannot obtain IBluetoothLowEnergy interface while disabled";
    return nullptr;
  }

  if (!low_energy_interface_.get())
    low_energy_interface_ = new BluetoothLowEnergyBinderServer(adapter_);

  return low_energy_interface_;
}

sp<IBluetoothGattClient>
BluetoothBinderServer::GetGattClientInterface() {
  VLOG(2) << __func__;

  if (!adapter_->IsEnabled()) {
    LOG(ERROR) << "Cannot obtain IBluetoothGattClient interface while disabled";
    return nullptr;
  }

  if (!gatt_client_interface_.get())
    gatt_client_interface_ = new BluetoothGattClientBinderServer(adapter_);

  return gatt_client_interface_;
}

sp<IBluetoothGattServer>
BluetoothBinderServer::GetGattServerInterface() {
  VLOG(2) << __func__;

  if (!adapter_->IsEnabled()) {
    LOG(ERROR) << "Cannot obtain IBluetoothGattServer interface while disabled";
    return nullptr;
  }

  if (!gatt_server_interface_.get())
    gatt_server_interface_ = new BluetoothGattServerBinderServer(adapter_);

  return gatt_server_interface_;
}

android::status_t BluetoothBinderServer::dump(int fd, const android::Vector<android::String16>& args) {
  VLOG(2) << __func__ << " called with fd " << fd;
  if  (args.size() > 0) {
    // TODO (jamuraa): Parse arguments and switch on --proto, --proto_text
    for (auto x : args) {
      VLOG(2) << __func__ << "argument: " << x.string();
    }
  }
  // TODO (jamuraa): enumerate profiles and dump profile information
  const bt_interface_t *iface = bluetooth::hal::BluetoothInterface::Get()->GetHALInterface();
  iface->dump(fd, NULL);
  return android::NO_ERROR;
}

void BluetoothBinderServer::OnAdapterStateChanged(
    bluetooth::Adapter* adapter,
    bluetooth::AdapterState prev_state,
    bluetooth::AdapterState new_state) {
  CHECK_EQ(adapter, adapter_);
  VLOG(2) << "Received adapter state update - prev: " << prev_state
          << " new: " << new_state;
  callbacks_.ForEach([prev_state, new_state](IBluetoothCallback* callback) {
    callback->OnBluetoothStateChange(prev_state, new_state);
  });
}

}  // namespace binder
}  // namespace ipc
