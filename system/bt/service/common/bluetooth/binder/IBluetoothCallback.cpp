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

#include "service/common/bluetooth/binder/IBluetoothCallback.h"

#include <base/logging.h>
#include <binder/Parcel.h>

using android::IBinder;
using android::Parcel;
using android::sp;
using android::status_t;

namespace ipc {
namespace binder {

// static
const char IBluetoothCallback::kServiceName[] =
    "bluetooth-callback-service";

// BnBluetoothCallback (server) implementation
// ========================================================

status_t BnBluetoothCallback::onTransact(
    uint32_t code,
    const Parcel& data,
    Parcel* reply,
    uint32_t flags) {
  VLOG(2) << "IBluetoothCallback transaction: " << code;
  if (!data.checkInterface(this))
    return android::PERMISSION_DENIED;

  switch (code) {
    case ON_BLUETOOTH_STATE_CHANGE_TRANSACTION: {
      int prev_state, new_state;
      if (data.readInt32(&prev_state) != android::NO_ERROR ||
          data.readInt32(&new_state) != android::NO_ERROR)
        return android::NOT_ENOUGH_DATA;

      OnBluetoothStateChange(
          static_cast<bluetooth::AdapterState>(prev_state),
          static_cast<bluetooth::AdapterState>(new_state));
      return android::NO_ERROR;
    }
    default:
      return BBinder::onTransact(code, data, reply, flags);
  }
}

// BpBluetoothCallback (client) implementation
// ========================================================

BpBluetoothCallback::BpBluetoothCallback(const sp<IBinder>& impl)
  : BpInterface<IBluetoothCallback>(impl) {
}

void BpBluetoothCallback::OnBluetoothStateChange(
    bluetooth::AdapterState prev_state,
    bluetooth::AdapterState new_state) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothCallback::getInterfaceDescriptor());
  data.writeInt32(prev_state);
  data.writeInt32(new_state);

  remote()->transact(IBluetoothCallback::ON_BLUETOOTH_STATE_CHANGE_TRANSACTION,
                     data, &reply);
}

IMPLEMENT_META_INTERFACE(BluetoothCallback, IBluetoothCallback::kServiceName);

}  // namespace binder
}  // namespace ipc
