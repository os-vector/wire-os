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

#include "service/common/bluetooth/binder/IBluetoothGattClientCallback.h"

#include <base/logging.h>
#include <binder/Parcel.h>

using android::IBinder;
using android::Parcel;
using android::sp;
using android::status_t;

namespace ipc {
namespace binder {

// static
const char IBluetoothGattClientCallback::kServiceName[] =
    "bluetooth-gatt-client-callback-service";

// BnBluetoothGattClientCallback (server) implementation
// ========================================================

status_t BnBluetoothGattClientCallback::onTransact(
    uint32_t code,
    const Parcel& data,
    Parcel* reply,
    uint32_t flags) {
  VLOG(2) << "IBluetoothGattClientCallback: " << code;
  if (!data.checkInterface(this))
    return android::PERMISSION_DENIED;

  switch (code) {
  case ON_CLIENT_REGISTERED_TRANSACTION: {
    int status = data.readInt32();
    int client_id = data.readInt32();
    OnClientRegistered(status, client_id);
    return android::NO_ERROR;
  }
  default:
    return BBinder::onTransact(code, data, reply, flags);
  }
}

// BpBluetoothGattClientCallback (client) implementation
// ========================================================

BpBluetoothGattClientCallback::BpBluetoothGattClientCallback(
    const sp<IBinder>& impl)
    : BpInterface<IBluetoothGattClientCallback>(impl) {
}

void BpBluetoothGattClientCallback::OnClientRegistered(
    int status, int client_id) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattClientCallback::getInterfaceDescriptor());
  data.writeInt32(status);
  data.writeInt32(client_id);

  remote()->transact(
      IBluetoothGattClientCallback::ON_CLIENT_REGISTERED_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

IMPLEMENT_META_INTERFACE(BluetoothGattClientCallback,
                         IBluetoothGattClientCallback::kServiceName);

}  // namespace binder
}  // namespace ipc
