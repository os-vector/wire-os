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

#include "service/common/bluetooth/binder/IBluetoothGattClient.h"

#include <base/logging.h>
#include <binder/Parcel.h>

using android::IBinder;
using android::interface_cast;
using android::Parcel;
using android::sp;
using android::status_t;

namespace ipc {
namespace binder {

// static
const char IBluetoothGattClient::kServiceName[] =
    "bluetooth-gatt-client-service";

// BnBluetoothGattClient (server) implementation
// ========================================================

status_t BnBluetoothGattClient::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
  VLOG(2) << "IBluetoothGattClient: " << code;
  if (!data.checkInterface(this))
    return android::PERMISSION_DENIED;

  switch (code) {
  case REGISTER_CLIENT_TRANSACTION: {
    sp<IBinder> callback = data.readStrongBinder();
    bool result = RegisterClient(
        interface_cast<IBluetoothGattClientCallback>(callback));
    reply->writeInt32(result);
    return android::NO_ERROR;
  }
  case UNREGISTER_CLIENT_TRANSACTION: {
    int client_id = data.readInt32();
    UnregisterClient(client_id);
    return android::NO_ERROR;
  }
  case UNREGISTER_ALL_TRANSACTION: {
    UnregisterAll();
    return android::NO_ERROR;
  }
  case REFRESH_DEVICE_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    bool result = RefreshDevice(client_id, address);
    reply->writeInt32(result);
    return android::NO_ERROR;
  }
  case REGISTER_FOR_NOTIFICATIONS_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    int handle = data.readInt32();
    bool result = SetCharacteristicNotification(client_id, address, handle, true);
    reply->writeInt32(result);
    return android::NO_ERROR;
  }
  case UNREGISTER_FOR_NOTIFICATIONS_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    int handle = data.readInt32();
    bool result = SetCharacteristicNotification(client_id, address, handle, false);
    reply->writeInt32(result);
    return android::NO_ERROR;
  }
  default:
    return BBinder::onTransact(code, data, reply, flags);
  }
}

// BpBluetoothGattClient (client) implementation
// ========================================================

BpBluetoothGattClient::BpBluetoothGattClient(const sp<IBinder>& impl)
    : BpInterface<IBluetoothGattClient>(impl) {
}

bool BpBluetoothGattClient::RegisterClient(
    const android::sp<IBluetoothGattClientCallback>& callback) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattClient::getInterfaceDescriptor());
  data.writeStrongBinder(IInterface::asBinder(callback.get()));

  remote()->transact(IBluetoothGattClient::REGISTER_CLIENT_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

void BpBluetoothGattClient::UnregisterClient(int client_id) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattClient::getInterfaceDescriptor());
  data.writeInt32(client_id);

  remote()->transact(IBluetoothGattClient::UNREGISTER_CLIENT_TRANSACTION,
                     data, &reply);
}

void BpBluetoothGattClient::UnregisterAll() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattClient::getInterfaceDescriptor());

  remote()->transact(IBluetoothGattClient::UNREGISTER_ALL_TRANSACTION,
                     data, &reply);
}

bool BpBluetoothGattClient::RefreshDevice(int client_id, const char* address) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattClient::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);

  remote()->transact(IBluetoothGattClient::REFRESH_DEVICE_TRANSACTION,
                     data, &reply);

  return reply.readInt32();

}
bool BpBluetoothGattClient::SetCharacteristicNotification(int client_id,
                                                          const char* address,
                                                          int handle,
                                                          bool enable) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattClient::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);
  data.writeInt32(handle);

  remote()->transact(enable ? REGISTER_FOR_NOTIFICATIONS_TRANSACTION :
                     UNREGISTER_FOR_NOTIFICATIONS_TRANSACTION,
                     data, &reply);
  
  return reply.readInt32();
}

IMPLEMENT_META_INTERFACE(BluetoothGattClient,
                         IBluetoothGattClient::kServiceName);

}  // namespace binder
}  // namespace ipc
