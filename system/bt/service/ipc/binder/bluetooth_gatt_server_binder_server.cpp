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

#include "service/ipc/binder/bluetooth_gatt_server_binder_server.h"

#include <base/logging.h>

#include "service/adapter.h"

namespace ipc {
namespace binder {

namespace {
const int kInvalidInstanceId = -1;
}  // namespace

BluetoothGattServerBinderServer::BluetoothGattServerBinderServer(
    bluetooth::Adapter* adapter) : adapter_(adapter) {
  CHECK(adapter_);
}

bool BluetoothGattServerBinderServer::RegisterServer(
    const android::sp<IBluetoothGattServerCallback>& callback) {
  VLOG(2) << __func__;
  bluetooth::GattServerFactory* gatt_server_factory =
      adapter_->GetGattServerFactory();

  return RegisterInstanceBase(callback, gatt_server_factory);
}

void BluetoothGattServerBinderServer::UnregisterServer(int server_id) {
  VLOG(2) << __func__;
  UnregisterInstanceBase(server_id);
}

void BluetoothGattServerBinderServer::UnregisterAll() {
  VLOG(2) << __func__;
  UnregisterAllBase();
}

bool BluetoothGattServerBinderServer::BeginServiceDeclaration(
    int server_id, bool is_primary, const bluetooth::UUID& uuid,
    std::unique_ptr<bluetooth::GattIdentifier>* out_id) {
  VLOG(2) << __func__;
  CHECK(out_id);
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    return false;
  }

  auto service_id = gatt_server->BeginServiceDeclaration(uuid, is_primary);
  if (!service_id) {
    LOG(ERROR) << "Failed to begin service declaration - server_id: "
               << server_id << " UUID: " << uuid.ToString();
    return false;
  }

  out_id->swap(service_id);

  return true;
}

bool BluetoothGattServerBinderServer::AddCharacteristic(
    int server_id, const bluetooth::UUID& uuid,
    int properties, int permissions,
    std::unique_ptr<bluetooth::GattIdentifier>* out_id) {
  VLOG(2) << __func__;
  CHECK(out_id);
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    return false;
  }

  auto char_id = gatt_server->AddCharacteristic(uuid, properties, permissions);
  if (!char_id) {
    LOG(ERROR) << "Failed to add characteristic - server_id: "
               << server_id << " UUID: " << uuid.ToString();
    return false;
  }

  out_id->swap(char_id);

  return true;
}

bool BluetoothGattServerBinderServer::AddDescriptor(
    int server_id, const bluetooth::UUID& uuid, int permissions,
    std::unique_ptr<bluetooth::GattIdentifier>* out_id) {
  VLOG(2) << __func__;
  CHECK(out_id);
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    return false;
  }

  auto desc_id = gatt_server->AddDescriptor(uuid, permissions);
  if (!desc_id) {
    LOG(ERROR) << "Failed to add descriptor - server_id: "
               << server_id << " UUID: " << uuid.ToString();
    return false;
  }

  out_id->swap(desc_id);

  return true;
}

bool BluetoothGattServerBinderServer::EndServiceDeclaration(int server_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    return false;
  }

  // Create a weak pointer and pass that to the callback to prevent a potential
  // use after free.
  android::wp<BluetoothGattServerBinderServer> weak_ptr_to_this(this);
  auto callback = [=](
      bluetooth::BLEStatus status,
      const bluetooth::GattIdentifier& service_id) {
    auto sp_to_this = weak_ptr_to_this.promote();
    if (!sp_to_this.get()) {
      VLOG(2) << "BluetoothLowEnergyBinderServer was deleted";
      return;
    }

    std::lock_guard<std::mutex> lock(*maps_lock());

    auto gatt_cb = GetGattServerCallback(server_id);
    if (!gatt_cb.get()) {
      VLOG(2) << "The callback was deleted";
      return;
    }

    gatt_cb->OnServiceAdded(status, service_id);
  };

  if (!gatt_server->EndServiceDeclaration(callback)) {
    LOG(ERROR) << "Failed to end service declaration";
    return false;
  }

  return true;
}

bool BluetoothGattServerBinderServer::SendResponse(
    int server_id, const std::string& device_address,
    int request_id, int status, int offset,
    const std::vector<uint8_t>& value) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    return false;
  }

  return gatt_server->SendResponse(
      device_address, request_id, static_cast<bluetooth::GATTError>(status),
      offset, value);
}

bool BluetoothGattServerBinderServer::SendNotification(
    int server_id,
    const std::string& device_address,
    const bluetooth::GattIdentifier& characteristic_id,
    bool confirm,
    const std::vector<uint8_t>& value) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    return false;
  }

  // Create a weak pointer and pass that to the callback to prevent a potential
  // use after free.
  android::wp<BluetoothGattServerBinderServer> weak_ptr_to_this(this);
  auto callback = [=](bluetooth::GATTError error) {
    auto sp_to_this = weak_ptr_to_this.promote();
    if (!sp_to_this.get()) {
      VLOG(2) << "BluetoothLowEnergyBinderServer was deleted";
      return;
    }

    std::lock_guard<std::mutex> lock(*maps_lock());

    auto gatt_cb = GetGattServerCallback(server_id);
    if (!gatt_cb.get()) {
      VLOG(2) << "The callback was deleted";
      return;
    }

    gatt_cb->OnNotificationSent(device_address, error);
  };

  if (!gatt_server->SendNotification(device_address, characteristic_id,
                                     confirm, value, callback)) {
    LOG(ERROR) << "Failed to send notification";
    return false;
  }

  return true;
}

void BluetoothGattServerBinderServer::OnCharacteristicReadRequest(
    bluetooth::GattServer* gatt_server,
    const std::string& device_address,
    int request_id, int offset, bool is_long,
    const bluetooth::GattIdentifier& characteristic_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnCharacteristicReadRequest(
      device_address, request_id, offset, is_long, characteristic_id);
}

void BluetoothGattServerBinderServer::OnDescriptorReadRequest(
    bluetooth::GattServer* gatt_server,
    const std::string& device_address,
    int request_id, int offset, bool is_long,
    const bluetooth::GattIdentifier& descriptor_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnDescriptorReadRequest(
      device_address, request_id, offset, is_long, descriptor_id);
}

android::sp<IBluetoothGattServerCallback>
BluetoothGattServerBinderServer::GetGattServerCallback(int server_id) {
  auto cb = GetCallback(server_id);
  return android::sp<IBluetoothGattServerCallback>(
      static_cast<IBluetoothGattServerCallback*>(cb.get()));
}

std::shared_ptr<bluetooth::GattServer>
BluetoothGattServerBinderServer::GetGattServer(int server_id) {
  return std::static_pointer_cast<bluetooth::GattServer>(
      GetInstance(server_id));
}

void BluetoothGattServerBinderServer::OnRegisterInstanceImpl(
    bluetooth::BLEStatus status,
    android::sp<IInterface> callback,
    bluetooth::BluetoothInstance* instance) {
  VLOG(1) << __func__ << " instance ID: " << instance->GetInstanceId()
          << " status: " << status;
  bluetooth::GattServer* gatt_server =
      static_cast<bluetooth::GattServer*>(instance);
  gatt_server->SetDelegate(this);

  android::sp<IBluetoothGattServerCallback> cb(
      static_cast<IBluetoothGattServerCallback*>(callback.get()));
  cb->OnServerRegistered(
      status,
      (status == bluetooth::BLE_STATUS_SUCCESS) ?
          instance->GetInstanceId() : kInvalidInstanceId);
}

void BluetoothGattServerBinderServer::OnCharacteristicWriteRequest(
    bluetooth::GattServer* gatt_server,
    const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& characteristic_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnCharacteristicWriteRequest(
      device_address, request_id, offset, is_prepare_write, need_response,
      value, characteristic_id);
}

void BluetoothGattServerBinderServer::OnDescriptorWriteRequest(
    bluetooth::GattServer* gatt_server,
    const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& descriptor_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnDescriptorWriteRequest(
      device_address, request_id, offset, is_prepare_write, need_response,
      value, descriptor_id);
}

void BluetoothGattServerBinderServer::OnExecuteWriteRequest(
    bluetooth::GattServer* gatt_server,
    const std::string& device_address,
    int request_id, bool is_execute) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnExecuteWriteRequest(device_address, request_id, is_execute);
}

}  // namespace binder
}  // namespace ipc
