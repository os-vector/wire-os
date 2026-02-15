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

#include "service/gatt_client.h"
#include "service/common/bluetooth/util/address_helper.h"

#include <base/logging.h>

using std::lock_guard;
using std::mutex;

namespace bluetooth {

// GattClient implementation
// ========================================================

GattClient::GattClient(const UUID& uuid, int client_id)
    : app_identifier_(uuid),
      client_id_(client_id) {
}

GattClient::~GattClient() {
  // Automatically unregister the client.
  VLOG(1) << "GattClient unregistering client: " << client_id_;

  hal::BluetoothGattInterface::Get()->GetClientHALInterface()->
      unregister_client(client_id_);
}

const UUID& GattClient::GetAppIdentifier() const {
  return app_identifier_;
}

int GattClient::GetInstanceId() const {
  return client_id_;
}

bool GattClient::RefreshDevice(std::string address) {
  VLOG(1) << "GattClient refreshing device " << address;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  bt_status_t status = hal::BluetoothGattInterface::Get()->
    GetClientHALInterface()->refresh(client_id_, &bda);

  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to refresh device failed";
    return false;
  }

  return true;
}

bool GattClient::SetCharacteristicNotification(std::string address, int handle,
                                               bool enable) {
  VLOG(1) << "GattClient SetCharacteristicNotification " << address
          << " handle: " << handle << " enable : " << enable;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  bt_status_t status;

  if (enable) {
    status = hal::BluetoothGattInterface::Get()->
        GetClientHALInterface()->register_for_notification(client_id_, &bda,
                                                           (uint16_t) handle);
  } else {
    status = hal::BluetoothGattInterface::Get()->
        GetClientHALInterface()->deregister_for_notification(client_id_, &bda,
                                                             (uint16_t) handle);
  }

  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to enable/disable characteristic notification failed";
    return false;
  }

  return true;
}

// GattClientFactory implementation
// ========================================================

GattClientFactory::GattClientFactory() {
  hal::BluetoothGattInterface::Get()->AddClientObserver(this);
}

GattClientFactory::~GattClientFactory() {
  hal::BluetoothGattInterface::Get()->RemoveClientObserver(this);
}

bool GattClientFactory::RegisterInstance(
    const UUID& uuid,
    const RegisterCallback& callback) {
  VLOG(1) << __func__ << " - UUID: " << uuid.ToString();
  lock_guard<mutex> lock(pending_calls_lock_);

  if (pending_calls_.find(uuid) != pending_calls_.end()) {
    LOG(ERROR) << "GATT client with given UUID already registered - "
               << "UUID: " << uuid.ToString();
    return false;
  }

  const btgatt_client_interface_t* hal_iface =
      hal::BluetoothGattInterface::Get()->GetClientHALInterface();
  bt_uuid_t app_uuid = uuid.GetBlueDroid();

  if (hal_iface->register_client(&app_uuid) != BT_STATUS_SUCCESS)
    return false;

  pending_calls_[uuid] = callback;

  return true;
}

void GattClientFactory::RegisterClientCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int status, int client_id,
    const bt_uuid_t& app_uuid) {
  UUID uuid(app_uuid);

  auto iter = pending_calls_.find(uuid);
  if (iter == pending_calls_.end()) {
    VLOG(1) << "Ignoring callback for unknown app_id: " << uuid.ToString();
    return;
  }

  bool success = (status == BT_STATUS_SUCCESS);
  BLEStatus result = success ? BLE_STATUS_SUCCESS : BLE_STATUS_FAILURE;

  // No need to construct a client if the call wasn't successful.
  std::unique_ptr<GattClient> client;
  if (success)
    client.reset(new GattClient(uuid, client_id));

  // Notify the result via the result callback.
  iter->second(result, uuid, std::move(client));

  pending_calls_.erase(iter);
}

}  // namespace bluetooth
